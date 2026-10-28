#ifndef CLOUD360_XBL_RUNTIME_H
#define CLOUD360_XBL_RUNTIME_H

#include "xbl.h"
#include "https.h"
#include "oauth_runtime.h"
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* POST callbacks are mockable; production uses verified curl-xenon HTTPS.
 * Never expose access, U, XSTS tokens, or user hashes to render snapshots. */
typedef Cloud360HttpsStatus (*Cloud360XblPost)(
    void *context, const Cloud360HttpsRequest *request,
    const char *json, Cloud360HttpsBody *body,
    long *http_code, size_t *received, int *error);

typedef struct {
    Cloud360XblStatus state;
    uint32_t error_code;
    long http_status;
    int transport_error;
    bool busy;
    uint32_t attempts;
} Cloud360XblView;

typedef struct {
    pthread_mutex_t mutex;
    bool initialized, busy;
    uint32_t generation, oauth_generation, attempts;
    Cloud360XblStatus state;
    uint32_t error_code;
    long http_status;
    int transport_error;
    char ca_file[CLOUD360_HTTPS_CA_BYTES];
    Cloud360XblPost post;
    void *post_context;
    Cloud360OAuthRuntime *oauth;
    /* Entire worker scratch space is statically allocated in title lifetime.
     * It is intentionally excluded from snapshots and console logging. */
    char oauth_access[CLOUD360_OAUTH_TOKEN_MAX];
    char request[CLOUD360_XBL_BODY_MAX];
    char response[CLOUD360_HTTPS_BODY_LIMIT+1];
    char user_token[CLOUD360_XBL_TOKEN_MAX];
    char xsts_token[CLOUD360_XBL_TOKEN_MAX];
    Cloud360XblReply parsed;
} Cloud360XblRuntime;

int cloud360_xbl_runtime_init(Cloud360XblRuntime *r, const char *ca_file,
                               Cloud360XblPost post, void *context);
bool cloud360_xbl_runtime_start(Cloud360XblRuntime *r,
                                 Cloud360OAuthRuntime *oauth);
void cloud360_xbl_runtime_cancel(Cloud360XblRuntime *r);
void cloud360_xbl_runtime_snapshot(Cloud360XblRuntime *r, Cloud360XblView *view);

#endif
