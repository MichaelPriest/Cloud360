#ifndef CLOUD360_OAUTH_H
#define CLOUD360_OAUTH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* RFC 8628 Device Authorization Grant. Pure C state machine.
 * No credentials or Microsoft client ID are hard-coded here. */
enum {
    CLOUD360_OAUTH_CODE_MAX = 32,
    CLOUD360_OAUTH_URI_MAX = 128,
    CLOUD360_OAUTH_SECRET_MAX = 1024,
    CLOUD360_OAUTH_INTERVAL_DEFAULT = 5,
    CLOUD360_OAUTH_INTERVAL_MAX = 120,
    CLOUD360_OAUTH_EXPIRY_MAX = 3600
};

typedef enum {
    CLOUD360_OAUTH_NOT_CONFIGURED = 0,
    CLOUD360_OAUTH_IDLE,
    CLOUD360_OAUTH_REQUESTING,
    CLOUD360_OAUTH_WAITING,
    CLOUD360_OAUTH_POLLING,
    CLOUD360_OAUTH_AUTHORIZED,
    CLOUD360_OAUTH_DECLINED,
    CLOUD360_OAUTH_EXPIRED,
    CLOUD360_OAUTH_CANCELLED,
    CLOUD360_OAUTH_UNAVAILABLE,
    CLOUD360_OAUTH_FAILED
} Cloud360OAuthStatus;

typedef enum {
    CLOUD360_OAUTH_REPLY_PENDING = 0,
    CLOUD360_OAUTH_REPLY_SLOW_DOWN,
    CLOUD360_OAUTH_REPLY_APPROVED,
    CLOUD360_OAUTH_REPLY_DENIED,
    CLOUD360_OAUTH_REPLY_EXPIRED,
    CLOUD360_OAUTH_REPLY_ERROR
} Cloud360OAuthReply;

typedef struct {
    char user_code[CLOUD360_OAUTH_CODE_MAX];
    char verification_uri[CLOUD360_OAUTH_URI_MAX];
    char device_code[CLOUD360_OAUTH_SECRET_MAX];
    uint32_t expires_in_seconds;
    uint32_t interval_seconds;
} Cloud360OAuthDevice;

typedef struct {
    Cloud360OAuthStatus status;
    Cloud360OAuthDevice device;
    uint32_t interval_ms;
    uint32_t expires_at;
    uint32_t next_poll_at;
    uint32_t attempts;
    uint32_t generation;
} Cloud360OAuth;

/* Snapshot contains ONLY user-displayable data, never device_code or tokens. */
typedef struct {
    Cloud360OAuthStatus status;
    char user_code[CLOUD360_OAUTH_CODE_MAX];
    char verification_uri[CLOUD360_OAUTH_URI_MAX];
    uint32_t remaining_seconds;
    uint32_t interval_seconds;
    uint32_t attempts;
} Cloud360OAuthView;

void cloud360_oauth_init(Cloud360OAuth *oauth, bool configured);
bool cloud360_oauth_begin(Cloud360OAuth *oauth);
bool cloud360_oauth_device(Cloud360OAuth *oauth, const Cloud360OAuthDevice *data,
                           uint32_t now_ms);
bool cloud360_oauth_due(Cloud360OAuth *oauth, uint32_t now_ms);
void cloud360_oauth_reply(Cloud360OAuth *oauth, Cloud360OAuthReply reply,
                         uint32_t now_ms);
void cloud360_oauth_tick(Cloud360OAuth *oauth, uint32_t now_ms);
void cloud360_oauth_cancel(Cloud360OAuth *oauth);
void cloud360_oauth_logout(Cloud360OAuth *oauth);
void cloud360_oauth_view(const Cloud360OAuth *oauth, uint32_t now_ms,
                          Cloud360OAuthView *view);
const char *cloud360_oauth_status_text(Cloud360OAuthStatus status);

#endif
