#include "oauth_runtime.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

enum {
    CLIENT_ID_BYTES = 36,
    MAX_FORM = 4096
};
static const char OAUTH_HOST[] = "login.microsoftonline.com";
static const char DEVICE_PATH[] = "/consumers/oauth2/v2.0/devicecode";
static const char TOKEN_PATH[] = "/consumers/oauth2/v2.0/token";
static const char SCOPES[] = "xboxlive.signin%20offline_access";

static void wipe(void *memory, size_t bytes)
{
    volatile unsigned char *p = memory;
    while (bytes--) *p++ = 0;
}
static bool hex_digit(char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}
static bool client_id_valid(const char *s)
{
    if (!s || strlen(s) != CLIENT_ID_BYTES) return false;
    for (size_t i=0;i<CLIENT_ID_BYTES;++i) {
        if (i==8||i==13||i==18||i==23) {
            if (s[i]!='-') return false;
        } else if (!hex_digit(s[i])) return false;
    }
    return true;
}

static bool read_id(char target[37], const char *path)
{
    if (!path) return false;
    FILE *f=fopen(path,"rb");
    if (!f) return false;
    char buffer[80]={0};
    bool ok=fgets(buffer,sizeof buffer,f)!=NULL;
    if (ok) {
        size_t n=strlen(buffer);
        while(n && (buffer[n-1]=='\n'||buffer[n-1]=='\r'))
            buffer[--n]=0;
        ok=client_id_valid(buffer);
        /* A second non-empty line is never allowed. */
        if (ok && fgetc(f)!=EOF)ok=false;
    }
    if(fclose(f)!=0)ok=false;
    if(ok)memcpy(target,buffer,37);
    wipe(buffer,sizeof buffer);
    return ok;
}
static bool encode_value(const char *input, char *out, size_t cap)
{
    static const char hexdigits[]="0123456789ABCDEF";
    size_t used=0;
    for(size_t i=0;input[i];++i){
        unsigned char c=(unsigned char)input[i];
        const bool allowed=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||
            (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~';
        if (allowed) {
            if (used+1>=cap) return false;
            out[used++]=(char)c;
        }else{
            if(used+3>=cap)return false;
            out[used++]='%';
            out[used++]=hexdigits[c>>4];
            out[used++]=hexdigits[c&15];
        }
    }
    if(used>=cap)return false;
    out[used]=0;
    return true;
}

static bool spawn(Cloud360OAuthRuntime *r, bool poll);
static void *worker(void *opaque)
{
    Cloud360OAuthRuntime *r=opaque;
    bool is_poll;
    uint32_t generation;
    Cloud360OAuthDevice device={0};
    char id[37],ca[CLOUD360_HTTPS_CA_BYTES];
    Cloud360OAuthTransport transport;

    (void)pthread_mutex_lock(&r->mutex);
    is_poll=r->worker_is_poll;
    generation=r->worker_generation;
    transport=r->transport;
    memcpy(id,r->client_id,sizeof id);
    memcpy(ca,r->ca_path,sizeof ca);
    if(is_poll) device=r->flow.device;
    (void)pthread_mutex_unlock(&r->mutex);

    char form[MAX_FORM]={0};
    bool valid=true;
    if(!is_poll) {
        const int n=snprintf(form,sizeof form,
            "client_id=%s&scope=%s",id,SCOPES);
        valid=n>0&&(size_t)n<sizeof form;
    }else{
        char coded[3*CLOUD360_OAUTH_SECRET_MAX+1]={0};
        valid=encode_value(device.device_code,coded,sizeof coded);
        if(valid){
            const int n=snprintf(form,sizeof form,
                "client_id=%s&grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Adevice_code&device_code=%s",
                id,coded);
            valid=n>0&&(size_t)n<sizeof form;
        }
        wipe(coded,sizeof coded);
    }
    Cloud360HttpsRequest request={0};
    (void)snprintf(request.host,sizeof request.host,"%s",OAUTH_HOST);
    (void)snprintf(request.path,sizeof request.path,"%s",
                   is_poll?TOKEN_PATH:DEVICE_PATH);
    (void)snprintf(request.ca_file,sizeof request.ca_file,"%s",ca);

    Cloud360OAuthDevice reply_device={0};
    Cloud360OAuthTokenResponse reply_token={0};
    Cloud360HttpsStatus result=CLOUD360_HTTPS_BAD_REQUEST;
    long code=0;
    size_t bytes=0;
    int error=0;
    bool parsed=false;

    if(valid){
        Cloud360HttpsBody body={
            .data=r->response,
            .capacity=sizeof r->response,
            .used=0
        };
        result=transport.post(transport.context,&request,form,&body,
                               &code,&bytes,&error);
        if((result==CLOUD360_HTTPS_OK ||
            (is_poll&&result==CLOUD360_HTTPS_HTTP_ERROR)) &&
           body.used==bytes){
            if(is_poll)
                parsed=cloud360_oauth_parse_token_json(body.data,body.used,
                                                       &reply_token);
            else if(code>=200&&code<300)
                parsed=cloud360_oauth_parse_device_json(body.data,body.used,
                                                         &reply_device);
        }
    }

    (void)pthread_mutex_lock(&r->mutex);
    r->http_status=code;
    r->transport_error=error;
    if(r->flow.generation==generation){
        if(!is_poll){
            if(result==CLOUD360_HTTPS_UNAVAILABLE ||
               result==CLOUD360_HTTPS_CA_MISSING) {
                r->flow.status=CLOUD360_OAUTH_UNAVAILABLE;
            } else if(!parsed || !cloud360_oauth_device(&r->flow,
                       &reply_device,r->now_ms)){
                r->flow.status=CLOUD360_OAUTH_FAILED;
            }
        }else{
            if(!parsed ||
               (reply_token.reply==CLOUD360_OAUTH_REPLY_APPROVED &&
                result!=CLOUD360_HTTPS_OK)) {
                cloud360_oauth_reply(&r->flow,CLOUD360_OAUTH_REPLY_ERROR,
                                     r->now_ms);
            }else{
                if(reply_token.reply==CLOUD360_OAUTH_REPLY_APPROVED) {
                    wipe(r->access_token,sizeof r->access_token);
                    wipe(r->refresh_token,sizeof r->refresh_token);
                    memcpy(r->access_token,reply_token.access_token,
                           sizeof r->access_token);
                    memcpy(r->refresh_token,reply_token.refresh_token,
                           sizeof r->refresh_token);
                }
                cloud360_oauth_reply(&r->flow,reply_token.reply,r->now_ms);
                if(r->flow.status!=CLOUD360_OAUTH_AUTHORIZED) {
                    wipe(r->access_token,sizeof r->access_token);
                    wipe(r->refresh_token,sizeof r->refresh_token);
                }
            }
        }
    }
    r->worker_busy=false;
    wipe(r->response,sizeof r->response);
    (void)pthread_mutex_unlock(&r->mutex);
    wipe(form,sizeof form);
    wipe(&reply_device,sizeof reply_device);
    wipe(&reply_token,sizeof reply_token);
    wipe(&device,sizeof device);
    wipe(id,sizeof id);
    return NULL;
}

static bool spawn(Cloud360OAuthRuntime *r, bool poll)
{
    if(r->worker_busy)return false;
    r->worker_busy=true;
    r->worker_is_poll=poll;
    r->worker_generation=r->flow.generation;
    pthread_t thread;
    int err=pthread_create(&thread,NULL,worker,r);
    if(err){
        r->worker_busy=false;
        r->transport_error=err;
        r->flow.status=CLOUD360_OAUTH_FAILED;
        return false;
    }
    (void)pthread_detach(thread);
    return true;
}

int cloud360_oauth_runtime_init(Cloud360OAuthRuntime *r,
                                 const char *client_file,const char *ca,
                                 Cloud360OAuthTransport transport)
{
    if(!r||!ca||!transport.post)return EINVAL;
    const size_t ca_len=strlen(ca);
    if(!ca_len||ca_len>=CLOUD360_HTTPS_CA_BYTES)return EINVAL;
    memset(r,0,sizeof *r);
    int err=pthread_mutex_init(&r->mutex,NULL);
    if(err)return err;
    r->initialized=true;
    r->transport=transport;
    memcpy(r->ca_path,ca,ca_len+1);
    cloud360_oauth_init(&r->flow,read_id(r->client_id,client_file));
    return 0;
}
bool cloud360_oauth_runtime_begin(Cloud360OAuthRuntime *r,uint32_t now_ms)
{
    if(!r||!r->initialized)return false;
    (void)pthread_mutex_lock(&r->mutex);
    bool ok=!r->worker_busy && cloud360_oauth_begin(&r->flow);
    r->now_ms=now_ms;
    if(ok){
        wipe(r->access_token,sizeof r->access_token);
        wipe(r->refresh_token,sizeof r->refresh_token);
        ok=spawn(r,false);
    }
    (void)pthread_mutex_unlock(&r->mutex);
    return ok;
}
void cloud360_oauth_runtime_tick(Cloud360OAuthRuntime *r,uint32_t now_ms)
{
    if(!r||!r->initialized)return;
    (void)pthread_mutex_lock(&r->mutex);
    r->now_ms=now_ms;
    cloud360_oauth_tick(&r->flow,now_ms);
    if(!r->worker_busy && cloud360_oauth_due(&r->flow,now_ms))
        (void)spawn(r,true);
    (void)pthread_mutex_unlock(&r->mutex);
}
void cloud360_oauth_runtime_cancel(Cloud360OAuthRuntime *r)
{
    if(!r||!r->initialized)return;
    (void)pthread_mutex_lock(&r->mutex);
    cloud360_oauth_cancel(&r->flow);
    wipe(r->access_token,sizeof r->access_token);
    wipe(r->refresh_token,sizeof r->refresh_token);
    (void)pthread_mutex_unlock(&r->mutex);
}
void cloud360_oauth_runtime_logout(Cloud360OAuthRuntime *r)
{
    if(!r||!r->initialized)return;
    (void)pthread_mutex_lock(&r->mutex);
    cloud360_oauth_logout(&r->flow);
    wipe(r->access_token,sizeof r->access_token);
    wipe(r->refresh_token,sizeof r->refresh_token);
    (void)pthread_mutex_unlock(&r->mutex);
}
void cloud360_oauth_runtime_view(Cloud360OAuthRuntime *r,uint32_t now_ms,
                                 Cloud360OAuthView *view)
{
    if(!view)return;
    if(!r||!r->initialized){
        cloud360_oauth_view(NULL,now_ms,view);
        view->status=CLOUD360_OAUTH_NOT_CONFIGURED;
        return;
    }
    (void)pthread_mutex_lock(&r->mutex);
    cloud360_oauth_view(&r->flow,now_ms,view);
    (void)pthread_mutex_unlock(&r->mutex);
}
const char *cloud360_oauth_runtime_hint(Cloud360OAuthRuntime *r)
{
    (void)r;
    return "Somente OAuth. Xbox Live/XSTS ainda indisponivel.";
}
