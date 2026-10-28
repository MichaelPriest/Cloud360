#include "xbl_runtime.h"
#include <errno.h>
#include <string.h>

static void clear_credentials(Cloud360XblRuntime *r)
{
    cloud360_xbl_wipe(r->oauth_access,sizeof r->oauth_access);
    cloud360_xbl_wipe(r->request,sizeof r->request);
    cloud360_xbl_wipe(r->response,sizeof r->response);
    cloud360_xbl_wipe(r->user_token,sizeof r->user_token);
    cloud360_xbl_wipe(r->xsts_token,sizeof r->xsts_token);
    cloud360_xbl_wipe(&r->parsed,sizeof r->parsed);
}

static bool oauth_still_authorized(Cloud360XblRuntime *r, uint32_t generation)
{
    Cloud360OAuthRuntime *oauth=r->oauth;
    if(!oauth || !oauth->initialized)return false;
    (void)pthread_mutex_lock(&oauth->mutex);
    bool valid=oauth->flow.status==CLOUD360_OAUTH_AUTHORIZED &&
               oauth->flow.generation==generation;
    (void)pthread_mutex_unlock(&oauth->mutex);
    return valid;
}

static void *xbl_worker(void *context)
{
    Cloud360XblRuntime *r=context;
    uint32_t generation=0, oauth_generation=0;
    (void)pthread_mutex_lock(&r->mutex);
    generation=r->generation;
    oauth_generation=r->oauth_generation;
    (void)pthread_mutex_unlock(&r->mutex);

    Cloud360XblStatus state=CLOUD360_XBL_FAILED;
    uint32_t xerr=0;
    long http_status=0;
    int transport_error=0;
    Cloud360HttpsStatus https=CLOUD360_HTTPS_BAD_REQUEST;
    Cloud360HttpsRequest request={0};
    (void)memcpy(request.ca_file,r->ca_file,sizeof request.ca_file);

    /* Phase 1: Microsoft OAuth access token -> Xbox U-token. */
    (void)memcpy(request.host,"user.auth.xboxlive.com",
                 sizeof "user.auth.xboxlive.com");
    (void)memcpy(request.path,"/user/authenticate",
                 sizeof "/user/authenticate");
    if(!cloud360_xbl_user_request(r->oauth_access,
                                  r->request,sizeof r->request))
        goto done;
    cloud360_xbl_wipe(r->oauth_access,sizeof r->oauth_access);
    Cloud360HttpsBody body={
        .data=r->response, .capacity=sizeof r->response, .used=0
    };
    size_t bytes=0;
    https=r->post(r->post_context,&request,r->request,&body,
                  &http_status,&bytes,&transport_error);
    cloud360_xbl_wipe(r->request,sizeof r->request);
    if(https==CLOUD360_HTTPS_CA_MISSING ||
       https==CLOUD360_HTTPS_UNAVAILABLE){
        state=CLOUD360_XBL_UNAVAILABLE;goto done;
    }
    if((https!=CLOUD360_HTTPS_OK &&
        https!=CLOUD360_HTTPS_HTTP_ERROR) ||
       body.used!=bytes){
        state=CLOUD360_XBL_FAILED;goto done;
    }
    if(!cloud360_xbl_parse_reply(body.data,body.used,&r->parsed)){
        state=CLOUD360_XBL_BAD_RESPONSE;goto done;
    }
    cloud360_xbl_wipe(r->response,sizeof r->response);
    if(r->parsed.has_error&&r->parsed.error_code){
        xerr=r->parsed.error_code;
        state=CLOUD360_XBL_REJECTED;goto done;
    }
    if(https!=CLOUD360_HTTPS_OK){
        state=CLOUD360_XBL_REJECTED;goto done;
    }
    (void)memcpy(r->user_token,r->parsed.token,sizeof r->user_token);
    cloud360_xbl_wipe(&r->parsed,sizeof r->parsed);

    if(!oauth_still_authorized(r,oauth_generation)){
        state=CLOUD360_XBL_CANCELLED;goto done;
    }
    (void)pthread_mutex_lock(&r->mutex);
    bool continue_xsts=r->generation==generation;
    if(continue_xsts)r->state=CLOUD360_XBL_REQUEST_XSTS;
    (void)pthread_mutex_unlock(&r->mutex);
    if(!continue_xsts){
        state=CLOUD360_XBL_CANCELLED;goto done;
    }

    /* Phase 2: U-token -> generic Xbox services XSTS token.
     * Xbox Cloud Gaming-specific relying parties are NOT used here. */
    (void)memset(&request,0,sizeof request);
    (void)memcpy(request.ca_file,r->ca_file,sizeof request.ca_file);
    (void)memcpy(request.host,"xsts.auth.xboxlive.com",
                 sizeof "xsts.auth.xboxlive.com");
    (void)memcpy(request.path,"/xsts/authorize",sizeof "/xsts/authorize");
    if(!cloud360_xbl_xsts_request(r->user_token,
                                  r->request,sizeof r->request))
        goto done;
    cloud360_xbl_wipe(r->user_token,sizeof r->user_token);
    body.used=0;
    bytes=0;http_status=0;transport_error=0;
    https=r->post(r->post_context,&request,r->request,&body,
                  &http_status,&bytes,&transport_error);
    cloud360_xbl_wipe(r->request,sizeof r->request);
    if(https==CLOUD360_HTTPS_CA_MISSING||
       https==CLOUD360_HTTPS_UNAVAILABLE){
        state=CLOUD360_XBL_UNAVAILABLE;goto done;
    }
    if((https!=CLOUD360_HTTPS_OK&&
        https!=CLOUD360_HTTPS_HTTP_ERROR)||
       body.used!=bytes){
        state=CLOUD360_XBL_FAILED;goto done;
    }
    if(!cloud360_xbl_parse_reply(body.data,body.used,&r->parsed)){
        state=CLOUD360_XBL_BAD_RESPONSE;goto done;
    }
    if(r->parsed.has_error&&r->parsed.error_code){
        xerr=r->parsed.error_code;
        state=CLOUD360_XBL_REJECTED;goto done;
    }
    if(https!=CLOUD360_HTTPS_OK){
        state=CLOUD360_XBL_REJECTED;goto done;
    }
    (void)memcpy(r->xsts_token,r->parsed.token,sizeof r->xsts_token);
    state=CLOUD360_XBL_AUTHORIZED;

done:
    /* We must check OAuth state without holding XBL lock to avoid lock
     * inversion with startup and log-out. The UI only sees a safe status. */
    if(!oauth_still_authorized(r,oauth_generation))
        state=CLOUD360_XBL_CANCELLED;
    (void)pthread_mutex_lock(&r->mutex);
    if(r->generation!=generation)
        state=CLOUD360_XBL_CANCELLED;
    if(state==CLOUD360_XBL_CANCELLED ||
       r->generation==generation){
        if(r->generation==generation){
            r->state=state;
            r->error_code=xerr;
            r->http_status=http_status;
            r->transport_error=transport_error;
        }
        if(state!=CLOUD360_XBL_AUTHORIZED)
            cloud360_xbl_wipe(r->xsts_token,sizeof r->xsts_token);
    }
    cloud360_xbl_wipe(r->oauth_access,sizeof r->oauth_access);
    cloud360_xbl_wipe(r->request,sizeof r->request);
    cloud360_xbl_wipe(r->response,sizeof r->response);
    cloud360_xbl_wipe(r->user_token,sizeof r->user_token);
    cloud360_xbl_wipe(&r->parsed,sizeof r->parsed);
    r->busy=false;
    (void)pthread_mutex_unlock(&r->mutex);
    return NULL;
}
int cloud360_xbl_runtime_init(Cloud360XblRuntime *r,const char *ca_file,
                               Cloud360XblPost post,void *context)
{
    if(!r||!ca_file||!post)return EINVAL;
    size_t n=strlen(ca_file);
    if(!n||n>=sizeof r->ca_file)return EINVAL;
    memset(r,0,sizeof *r);
    int err=pthread_mutex_init(&r->mutex,NULL);
    if(err)return err;
    r->initialized=true;
    memcpy(r->ca_file,ca_file,n+1);
    r->post=post;
    r->post_context=context;
    r->state=CLOUD360_XBL_IDLE;
    return 0;
}
bool cloud360_xbl_runtime_start(Cloud360XblRuntime *r,
                                 Cloud360OAuthRuntime *oauth)
{
    if(!r||!r->initialized||!oauth||!oauth->initialized)return false;
    (void)pthread_mutex_lock(&oauth->mutex);
    if(oauth->flow.status!=CLOUD360_OAUTH_AUTHORIZED ||
       !oauth->access_token[0]){
        (void)pthread_mutex_unlock(&oauth->mutex);
        return false;
    }
    (void)pthread_mutex_lock(&r->mutex);
    if(r->busy){
        (void)pthread_mutex_unlock(&r->mutex);
        (void)pthread_mutex_unlock(&oauth->mutex);
        return false;
    }
    clear_credentials(r);
    memcpy(r->oauth_access,oauth->access_token,sizeof r->oauth_access);
    r->oauth_generation=oauth->flow.generation;
    r->oauth=oauth;
    r->state=CLOUD360_XBL_REQUEST_USER;
    r->error_code=0;
    r->http_status=0;
    r->transport_error=0;
    ++r->generation;
    ++r->attempts;
    r->busy=true;
    pthread_t thread;
    int err=pthread_create(&thread,NULL,xbl_worker,r);
    if(err){
        r->busy=false;
        r->state=CLOUD360_XBL_FAILED;
        r->transport_error=err;
        clear_credentials(r);
    }else{
        (void)pthread_detach(thread);
    }
    (void)pthread_mutex_unlock(&r->mutex);
    (void)pthread_mutex_unlock(&oauth->mutex);
    return err==0;
}
void cloud360_xbl_runtime_cancel(Cloud360XblRuntime *r)
{
    if(!r||!r->initialized)return;
    (void)pthread_mutex_lock(&r->mutex);
    ++r->generation;
    r->state=CLOUD360_XBL_CANCELLED;
    /* Never wipe an active worker's buffer from the UI thread.
     * Its returned result is ignored on generation mismatch. */
    if(!r->busy)clear_credentials(r);
    else cloud360_xbl_wipe(r->xsts_token,sizeof r->xsts_token);
    (void)pthread_mutex_unlock(&r->mutex);
}
void cloud360_xbl_runtime_snapshot(Cloud360XblRuntime *r,Cloud360XblView *view)
{
    if(!view)return;
    memset(view,0,sizeof *view);
    if(!r||!r->initialized)return;
    (void)pthread_mutex_lock(&r->mutex);
    view->state=r->state;
    view->error_code=r->error_code;
    view->http_status=r->http_status;
    view->transport_error=r->transport_error;
    view->busy=r->busy;
    view->attempts=r->attempts;
    (void)pthread_mutex_unlock(&r->mutex);
}
