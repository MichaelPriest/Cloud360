#ifndef CLOUD360_XBL_H
#define CLOUD360_XBL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Generic Xbox services XAU/XSTS handshake, not xCloud entitlement.
 * Treat all tokens as opaque credentials, never persist or log them. */
enum { CLOUD360_XBL_TOKEN_MAX=8192, CLOUD360_XBL_UHS_MAX=64,
       CLOUD360_XBL_BODY_MAX=10000 };
typedef enum {
    CLOUD360_XBL_IDLE=0,
    CLOUD360_XBL_REQUEST_USER,
    CLOUD360_XBL_REQUEST_XSTS,
    CLOUD360_XBL_AUTHORIZED,
    CLOUD360_XBL_REJECTED,
    CLOUD360_XBL_UNAVAILABLE,
    CLOUD360_XBL_BAD_RESPONSE,
    CLOUD360_XBL_FAILED,
    CLOUD360_XBL_CANCELLED
} Cloud360XblStatus;

typedef struct {
    char token[CLOUD360_XBL_TOKEN_MAX];
    char user_hash[CLOUD360_XBL_UHS_MAX];
    uint32_t error_code;
    bool has_error;
} Cloud360XblReply;

void cloud360_xbl_wipe(void *data, size_t n);
bool cloud360_xbl_user_request(const char *ms_token, char *out, size_t capacity);
bool cloud360_xbl_xsts_request(const char *user_token, char *out, size_t capacity);
bool cloud360_xbl_parse_reply(const char *json, size_t length,
                              Cloud360XblReply *out);
const char *cloud360_xbl_status_text(Cloud360XblStatus status);
const char *cloud360_xbl_error_text(uint32_t code);
#endif
