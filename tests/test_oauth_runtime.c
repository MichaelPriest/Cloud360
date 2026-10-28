#define _POSIX_C_SOURCE 200809L
#include "../src/oauth_runtime.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int device_calls;
    int token_calls;
    int step;
    bool gate;
    bool in_call;
    bool offline;
} Fake;

static Cloud360OAuthRuntime auth;
static Fake fake;

static Cloud360HttpsStatus mock_post(
    void *context, const Cloud360HttpsRequest *request,
    const char *form, Cloud360HttpsBody *body,
    long *http_code, size_t *bytes, int *error)
{
    Fake *f=context;
    assert(strcmp(request->host,"login.microsoftonline.com")==0);
    assert(strcmp(request->ca_file,"game:/cacert.pem")==0);
    assert(strstr(form,"client_id=12345678-1234-1234-1234-123456789abc"));
    assert(strstr(form,"password=")==NULL);
    assert(strstr(form,"refresh_token=")==NULL);

    (void)pthread_mutex_lock(&f->mutex);
    f->in_call=true;
    (void)pthread_cond_broadcast(&f->condition);
    while(f->gate)
        (void)pthread_cond_wait(&f->condition,&f->mutex);
    const bool offline=f->offline;
    char json[4096]={0};
    int current=f->step;
    if(strstr(request->path,"devicecode")) {
        assert(strstr(form,"scope=xboxlive.signin%20offline_access"));
        ++f->device_calls;
        strcpy(json,"{\"device_code\":\"DEVICE+SECRET\",\"user_code\":\"ABCD-1234\","
                    "\"verification_uri\":\"https://microsoft.com/devicelogin\","
                    "\"expires_in\":900,\"interval\":5}");
    } else {
        assert(strstr(request->path,"token"));
        assert(strstr(form,"device_code=DEVICE%2BSECRET"));
        ++f->token_calls;
        if(current==0)strcpy(json,"{\"error\":\"authorization_pending\"}");
        else if(current==1)strcpy(json,"{\"error\":\"slow_down\"}");
        else strcpy(json,"{\"token_type\":\"Bearer\","
                    "\"access_token\":\"FAKE_ACCESS_NOT_REAL\","
                    "\"refresh_token\":\"FAKE_REFRESH_NOT_REAL\"}");
        ++f->step;
    }
    (void)pthread_mutex_unlock(&f->mutex);
    if(offline){
        *http_code=0;*bytes=0;*error=ENETDOWN;
        return CLOUD360_HTTPS_UNAVAILABLE;
    }
    *http_code=strstr(request->path,"devicecode") || current>=2 ? 200 : 400;
    *error=0;
    const size_t len=strlen(json);
    assert(len<body->capacity);
    memcpy(body->data,json,len+1);
    body->used=*bytes=len;
    return *http_code==200 ? CLOUD360_HTTPS_OK : CLOUD360_HTTPS_HTTP_ERROR;
}

static Cloud360OAuthView await_status(Cloud360OAuthStatus expected,
                                      uint32_t now_ms)
{
    const struct timespec pause={.tv_sec=0,.tv_nsec=5000000};
    Cloud360OAuthView v={0};
    for(int n=0;n<300;++n){
        cloud360_oauth_runtime_view(&auth,now_ms,&v);
        if(v.status==expected)return v;
        (void)nanosleep(&pause,NULL);
    }
    fprintf(stderr,"OAuth worker timeout: got %d expected %d\n",
            (int)v.status,(int)expected);
    assert(0);
    return v;
}
static void wait_call(void)
{
    (void)pthread_mutex_lock(&fake.mutex);
    while(!fake.in_call)
        (void)pthread_cond_wait(&fake.condition,&fake.mutex);
    (void)pthread_mutex_unlock(&fake.mutex);
}
static void wait_not_busy(void)
{
    const struct timespec pause={.tv_sec=0,.tv_nsec=5000000};
    for(int i=0;i<200;++i){
        (void)pthread_mutex_lock(&auth.mutex);
        const bool done=!auth.worker_busy;
        (void)pthread_mutex_unlock(&auth.mutex);
        if(done)return;
        (void)nanosleep(&pause,NULL);
    }
    assert(0 && "worker remained busy");
}

int main(void)
{
    assert(pthread_mutex_init(&fake.mutex,NULL)==0);
    assert(pthread_cond_init(&fake.condition,NULL)==0);
    const Cloud360OAuthTransport transport={mock_post,&fake};
    static Cloud360OAuthRuntime unconfigured;
    assert(cloud360_oauth_runtime_init(&unconfigured,
            "/tmp/nonexistent-cloud360-ms-client.txt",
            "game:/cacert.pem",transport)==0);
    Cloud360OAuthView view;
    cloud360_oauth_runtime_view(&unconfigured,0,&view);
    assert(view.status==CLOUD360_OAUTH_NOT_CONFIGURED);
    assert(!cloud360_oauth_runtime_begin(&unconfigured,0));

    const char *path="/tmp/cloud360-ms-client-test.txt";
    FILE *file=fopen(path,"wb");
    assert(file);
    const char *id="12345678-1234-1234-1234-123456789abc\n";
    assert(fwrite(id,1,strlen(id),file)==strlen(id));
    assert(fclose(file)==0);
    assert(cloud360_oauth_runtime_init(&auth,path,
                                      "game:/cacert.pem",transport)==0);
    assert(remove(path)==0);

    cloud360_oauth_runtime_view(&auth,1000,&view);
    assert(view.status==CLOUD360_OAUTH_IDLE);
    assert(cloud360_oauth_runtime_begin(&auth,1000));
    view=await_status(CLOUD360_OAUTH_WAITING,1000);
    assert(!strcmp(view.user_code,"ABCD-1234"));
    assert(!strcmp(view.verification_uri,"https://microsoft.com/devicelogin"));
    assert(view.remaining_seconds==900);
    cloud360_oauth_runtime_tick(&auth,5999);
    assert(fake.token_calls==0);
    cloud360_oauth_runtime_tick(&auth,6000);
    view=await_status(CLOUD360_OAUTH_WAITING,6000);
    assert(view.attempts==1 && fake.token_calls==1);
    cloud360_oauth_runtime_tick(&auth,10999);
    assert(fake.token_calls==1);
    cloud360_oauth_runtime_tick(&auth,11000);
    view=await_status(CLOUD360_OAUTH_WAITING,11000);
    assert(view.interval_seconds==10 && fake.token_calls==2);
    cloud360_oauth_runtime_tick(&auth,20999);
    assert(fake.token_calls==2);
    cloud360_oauth_runtime_tick(&auth,21000);
    view=await_status(CLOUD360_OAUTH_AUTHORIZED,21000);
    assert(view.user_code[0]==0);
    assert(fake.token_calls==3);
    assert(!strcmp(auth.access_token,"FAKE_ACCESS_NOT_REAL"));
    assert(!strcmp(auth.refresh_token,"FAKE_REFRESH_NOT_REAL"));
    cloud360_oauth_runtime_logout(&auth);
    assert(auth.access_token[0]==0 && auth.refresh_token[0]==0);
    cloud360_oauth_runtime_view(&auth,22000,&view);
    assert(view.status==CLOUD360_OAUTH_IDLE);

    /* An in-flight cancellation must not resurrect a code or credentials. */
    (void)pthread_mutex_lock(&fake.mutex);
    fake.gate=true;
    fake.in_call=false;
    (void)pthread_mutex_unlock(&fake.mutex);
    assert(cloud360_oauth_runtime_begin(&auth,22000));
    wait_call();
    cloud360_oauth_runtime_cancel(&auth);
    cloud360_oauth_runtime_view(&auth,22000,&view);
    assert(view.status==CLOUD360_OAUTH_CANCELLED);
    assert(!cloud360_oauth_runtime_begin(&auth,22001)); /* one worker at a time */
    (void)pthread_mutex_lock(&fake.mutex);
    fake.gate=false;
    (void)pthread_cond_broadcast(&fake.condition);
    (void)pthread_mutex_unlock(&fake.mutex);
    wait_not_busy();
    cloud360_oauth_runtime_view(&auth,23000,&view);
    assert(view.status==CLOUD360_OAUTH_CANCELLED && view.user_code[0]==0);

    (void)pthread_mutex_lock(&fake.mutex);
    fake.offline=true;
    (void)pthread_mutex_unlock(&fake.mutex);
    assert(cloud360_oauth_runtime_begin(&auth,24000));
    view=await_status(CLOUD360_OAUTH_UNAVAILABLE,24000);
    assert(view.user_code[0]==0);
    assert(auth.access_token[0]==0);

    assert(pthread_cond_destroy(&fake.condition)==0);
    assert(pthread_mutex_destroy(&fake.mutex)==0);
    puts("Cloud360 async OAuth device login, polling and cancellation: PASS");
    return 0;
}
