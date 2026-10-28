#define _POSIX_C_SOURCE 200809L
#include "../src/xbl_runtime.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t signal;
    bool gate,entered,deny,tls_off;
    unsigned calls;
} Fake;

static Fake fake;
static Cloud360OAuthRuntime oauth;
static Cloud360XblRuntime xbl;

static Cloud360HttpsStatus send_mock(
    void *context,const Cloud360HttpsRequest *req,const char *body,
    Cloud360HttpsBody *out,long *http_status,size_t *bytes,int *error)
{
    Fake *f=context;
    assert(!strcmp(req->ca_file,"game:/cacert.pem"));
    assert(strstr(body,"FAKE")!=NULL);
    assert(strstr(body,"client_secret")==NULL);
    (void)pthread_mutex_lock(&f->mutex);
    f->entered=true;
    ++f->calls;
    (void)pthread_cond_broadcast(&f->signal);
    while(f->gate)(void)pthread_cond_wait(&f->signal,&f->mutex);
    const bool unavailable=f->tls_off;
    const bool deny=f->deny;
    (void)pthread_mutex_unlock(&f->mutex);

    *http_status=200; *error=0;
    const char *json=NULL;
    if(!strcmp(req->host,"user.auth.xboxlive.com")) {
        assert(!strcmp(req->path,"/user/authenticate"));
        assert(strstr(body,"RpsTicket\":\"d=FAKE_MS_OAUTH\""));
        json="{\"Token\":\"FAKE_USER_TOKEN\","
             "\"DisplayClaims\":{\"xui\":[{\"uhs\":\"fake-hash\"}]}}";
    } else {
        assert(!strcmp(req->host,"xsts.auth.xboxlive.com"));
        assert(!strcmp(req->path,"/xsts/authorize"));
        assert(strstr(body,"UserTokens\":[\"FAKE_USER_TOKEN\"]"));
        json=deny?"{\"XErr\":2148916233}":
          "{\"Token\":\"FAKE_XSTS_TOKEN\","
          "\"DisplayClaims\":{\"xui\":[{\"uhs\":\"fake-hash\"}]}}";
    }
    if(unavailable) {
        *http_status=0;*bytes=0;*error=ENOSYS;
        return CLOUD360_HTTPS_UNAVAILABLE;
    }
    if(deny && !strcmp(req->host,"xsts.auth.xboxlive.com"))
        *http_status=401;
    size_t n=strlen(json);
    assert(n<out->capacity);
    memcpy(out->data,json,n+1);
    out->used=*bytes=n;
    return *http_status==200 ? CLOUD360_HTTPS_OK :
                               CLOUD360_HTTPS_HTTP_ERROR;
}
static Cloud360XblView wait_done(void)
{
    const struct timespec pause={.tv_sec=0,.tv_nsec=5000000};
    Cloud360XblView result={0};
    for(int i=0;i<300;++i){
        cloud360_xbl_runtime_snapshot(&xbl,&result);
        if(!result.busy)return result;
        (void)nanosleep(&pause,NULL);
    }
    assert(0&&"Xbox XSTS worker timeout");
    return result;
}
static void authenticate_oauth(void)
{
    (void)pthread_mutex_lock(&oauth.mutex);
    oauth.flow.status=CLOUD360_OAUTH_AUTHORIZED;
    ++oauth.flow.generation;
    strcpy(oauth.access_token,"FAKE_MS_OAUTH");
    (void)pthread_mutex_unlock(&oauth.mutex);
}
static void cancel_oauth(void)
{
    (void)pthread_mutex_lock(&oauth.mutex);
    oauth.flow.status=CLOUD360_OAUTH_CANCELLED;
    ++oauth.flow.generation;
    memset(oauth.access_token,0,sizeof oauth.access_token);
    (void)pthread_mutex_unlock(&oauth.mutex);
}
int main(void)
{
    assert(pthread_mutex_init(&fake.mutex,NULL)==0);
    assert(pthread_cond_init(&fake.signal,NULL)==0);
    assert(pthread_mutex_init(&oauth.mutex,NULL)==0);
    oauth.initialized=true;
    authenticate_oauth();

    assert(cloud360_xbl_runtime_init(&xbl,"game:/cacert.pem",
                                      send_mock,&fake)==0);
    assert(cloud360_xbl_runtime_start(&xbl,&oauth));
    Cloud360XblView view=wait_done();
    assert(view.state==CLOUD360_XBL_AUTHORIZED);
    assert(view.attempts==1 && fake.calls==2);
    assert(!strcmp(xbl.xsts_token,"FAKE_XSTS_TOKEN"));
    assert(xbl.oauth_access[0]==0 && xbl.user_token[0]==0);
    assert(view.error_code==0);
    cloud360_xbl_runtime_cancel(&xbl);
    assert(xbl.xsts_token[0]==0);

    fake.deny=true;
    assert(cloud360_xbl_runtime_start(&xbl,&oauth));
    view=wait_done();
    assert(view.state==CLOUD360_XBL_REJECTED);
    assert(view.error_code==2148916233u);
    assert(xbl.xsts_token[0]==0);
    fake.deny=false;

    fake.tls_off=true;
    assert(cloud360_xbl_runtime_start(&xbl,&oauth));
    view=wait_done();
    assert(view.state==CLOUD360_XBL_UNAVAILABLE);
    assert(xbl.xsts_token[0]==0);
    fake.tls_off=false;

    (void)pthread_mutex_lock(&fake.mutex);
    fake.gate=true;fake.entered=false;
    (void)pthread_mutex_unlock(&fake.mutex);
    assert(cloud360_xbl_runtime_start(&xbl,&oauth));
    (void)pthread_mutex_lock(&fake.mutex);
    while(!fake.entered)
        (void)pthread_cond_wait(&fake.signal,&fake.mutex);
    (void)pthread_mutex_unlock(&fake.mutex);
    assert(!cloud360_xbl_runtime_start(&xbl,&oauth));
    cloud360_xbl_runtime_cancel(&xbl);
    cancel_oauth();
    (void)pthread_mutex_lock(&fake.mutex);
    fake.gate=false;
    (void)pthread_cond_broadcast(&fake.signal);
    (void)pthread_mutex_unlock(&fake.mutex);
    view=wait_done();
    assert(view.state==CLOUD360_XBL_CANCELLED);
    assert(xbl.xsts_token[0]==0);
    assert(!cloud360_xbl_runtime_start(&xbl,&oauth));
    puts("Cloud360 Xbox U-token/XSTS async, denial, TLS fail closed, cancel: PASS");
    return 0;
}
