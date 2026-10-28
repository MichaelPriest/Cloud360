#ifndef CLOUD360_OAUTH_RUNTIME_H
#define CLOUD360_OAUTH_RUNTIME_H
#include "oauth.h"
#include "oauth_json.h"
#include "https.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/* Registered Microsoft public client ID is supplied locally, never borrowed
 * from another app or committed to GitHub. This is an OAuth-only experiment:
 * Xbox Live/XSTS/xCloud authorization is NOT yet integrated. */
typedef Cloud360HttpsStatus (*Cloud360OAuthPost)(
    void *context, const Cloud360HttpsRequest *request,
    const char *form, Cloud360HttpsBody *body,
    long *status, size_t *bytes, int *transport_error);

typedef struct {
    Cloud360OAuthPost post;
    void *context;
} Cloud360OAuthTransport;

typedef struct {
    pthread_mutex_t mutex;
    bool initialized;
    bool worker_busy;
    bool worker_is_poll;
    Cloud360OAuth flow;
    uint32_t now_ms;
    uint32_t worker_generation;
    int transport_error;
    long http_status;
    Cloud360OAuthTransport transport;
    char client_id[37];
    char ca_path[CLOUD360_HTTPS_CA_BYTES];
    char response[CLOUD360_HTTPS_BODY_LIMIT + 1];
    /* Never copied into the UI, logs, files, or the GitHub repo. */
    char access_token[CLOUD360_OAUTH_TOKEN_MAX];
    char refresh_token[CLOUD360_OAUTH_TOKEN_MAX];
} Cloud360OAuthRuntime;

int cloud360_oauth_runtime_init(Cloud360OAuthRuntime *runtime,
                                 const char *client_id_path,
                                 const char *ca_path,
                                 Cloud360OAuthTransport transport);
bool cloud360_oauth_runtime_begin(Cloud360OAuthRuntime *runtime,
                                   uint32_t now_ms);
void cloud360_oauth_runtime_tick(Cloud360OAuthRuntime *runtime, uint32_t now_ms);
void cloud360_oauth_runtime_cancel(Cloud360OAuthRuntime *runtime);
void cloud360_oauth_runtime_logout(Cloud360OAuthRuntime *runtime);
void cloud360_oauth_runtime_view(Cloud360OAuthRuntime *runtime, uint32_t now_ms,
                                 Cloud360OAuthView *view);
const char *cloud360_oauth_runtime_hint(Cloud360OAuthRuntime *runtime);
#endif
