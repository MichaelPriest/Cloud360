#ifndef CLOUD360_OAUTH_JSON_H
#define CLOUD360_OAUTH_JSON_H
#include "oauth.h"
#include <stdbool.h>
#include <stddef.h>

enum { CLOUD360_OAUTH_TOKEN_MAX = 4096 };

typedef struct {
    Cloud360OAuthReply reply;
    char access_token[CLOUD360_OAUTH_TOKEN_MAX];
    char refresh_token[CLOUD360_OAUTH_TOKEN_MAX];
} Cloud360OAuthTokenResponse;

/* Parse bounded JSON from a trusted TLS channel; never print its contents.
 * Only known top-level keys are extracted. Nested unknown keys are skipped. */
bool cloud360_oauth_parse_device_json(const char *json, size_t bytes,
                                      Cloud360OAuthDevice *out);
bool cloud360_oauth_parse_token_json(const char *json, size_t bytes,
                                     Cloud360OAuthTokenResponse *out);
#endif
