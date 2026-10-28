#include "oauth.h"
#include <string.h>

/* Clear secrets even under link-time optimization; never log device_code. */
static void clear_secret(void *value, size_t count)
{
    volatile unsigned char *p = value;
    while (count--) *p++ = 0;
}

static bool finished(Cloud360OAuthStatus status)
{
    return status == CLOUD360_OAUTH_AUTHORIZED ||
           status == CLOUD360_OAUTH_DECLINED ||
           status == CLOUD360_OAUTH_EXPIRED ||
           status == CLOUD360_OAUTH_CANCELLED ||
           status == CLOUD360_OAUTH_UNAVAILABLE ||
           status == CLOUD360_OAUTH_FAILED;
}

static size_t bounded(const char *s, size_t n)
{
    size_t len = 0;
    while (len < n && s[len]) ++len;
    return len;
}

/* Device code is an opaque secret. User code + URI are display-only. */
static bool valid_device(const Cloud360OAuthDevice *d)
{
    if (!d) return false;
    const size_t code = bounded(d->user_code, sizeof d->user_code);
    const size_t uri = bounded(d->verification_uri, sizeof d->verification_uri);
    const size_t secret = bounded(d->device_code, sizeof d->device_code);
    if (!code || code >= sizeof d->user_code ||
        uri < 12 || uri >= sizeof d->verification_uri ||
        !secret || secret >= sizeof d->device_code ||
        d->expires_in_seconds < 30 ||
        d->expires_in_seconds > CLOUD360_OAUTH_EXPIRY_MAX ||
        d->interval_seconds > CLOUD360_OAUTH_INTERVAL_MAX)
        return false;

    if (memcmp(d->verification_uri, "https://", 8) != 0)
        return false;
    for (size_t i = 0; i < code; ++i) {
        unsigned char c = (unsigned char)d->user_code[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    }
    for (size_t i = 0; i < uri; ++i) {
        unsigned char c = (unsigned char)d->verification_uri[i];
        if (c < 33 || c > 126 || c == '\\' || c == '"' || c == '\'' ||
            c == '<' || c == '>')
            return false;
    }
    return true;
}

void cloud360_oauth_init(Cloud360OAuth *oauth, bool configured)
{
    if (!oauth) return;
    memset(oauth, 0, sizeof *oauth);
    oauth->status = configured ? CLOUD360_OAUTH_IDLE :
                                 CLOUD360_OAUTH_NOT_CONFIGURED;
}

bool cloud360_oauth_begin(Cloud360OAuth *oauth)
{
    if (!oauth || oauth->status == CLOUD360_OAUTH_NOT_CONFIGURED ||
        oauth->status == CLOUD360_OAUTH_REQUESTING ||
        oauth->status == CLOUD360_OAUTH_WAITING ||
        oauth->status == CLOUD360_OAUTH_POLLING)
        return false;
    clear_secret(&oauth->device, sizeof oauth->device);
    oauth->interval_ms = 0;
    oauth->expires_at = 0;
    oauth->next_poll_at = 0;
    oauth->attempts = 0;
    ++oauth->generation;
    oauth->status = CLOUD360_OAUTH_REQUESTING;
    return true;
}

bool cloud360_oauth_device(Cloud360OAuth *oauth, const Cloud360OAuthDevice *data,
                           uint32_t now_ms)
{
    if (!oauth || oauth->status != CLOUD360_OAUTH_REQUESTING ||
        !valid_device(data))
        return false;
    oauth->device = *data;
    oauth->interval_ms = (data->interval_seconds ?
        data->interval_seconds : CLOUD360_OAUTH_INTERVAL_DEFAULT) * 1000;
    oauth->expires_at = now_ms + data->expires_in_seconds * 1000;
    oauth->next_poll_at = now_ms + oauth->interval_ms;
    oauth->status = CLOUD360_OAUTH_WAITING;
    return true;
}

void cloud360_oauth_tick(Cloud360OAuth *oauth, uint32_t now_ms)
{
    if (!oauth) return;
    if ((oauth->status == CLOUD360_OAUTH_WAITING ||
         oauth->status == CLOUD360_OAUTH_POLLING) &&
        (int32_t)(now_ms - oauth->expires_at) >= 0) {
        clear_secret(&oauth->device, sizeof oauth->device);
        oauth->status = CLOUD360_OAUTH_EXPIRED;
    }
}

bool cloud360_oauth_due(Cloud360OAuth *oauth, uint32_t now_ms)
{
    if (!oauth) return false;
    cloud360_oauth_tick(oauth, now_ms);
    if (oauth->status != CLOUD360_OAUTH_WAITING ||
        (int32_t)(now_ms - oauth->next_poll_at) < 0)
        return false;
    oauth->status = CLOUD360_OAUTH_POLLING;
    ++oauth->attempts;
    return true;
}

void cloud360_oauth_reply(Cloud360OAuth *oauth, Cloud360OAuthReply reply,
                          uint32_t now_ms)
{
    if (!oauth || oauth->status != CLOUD360_OAUTH_POLLING) return;
    cloud360_oauth_tick(oauth, now_ms);
    if (oauth->status != CLOUD360_OAUTH_POLLING) return;

    if (reply == CLOUD360_OAUTH_REPLY_APPROVED) {
        oauth->status = CLOUD360_OAUTH_AUTHORIZED;
        clear_secret(&oauth->device, sizeof oauth->device);
        return;
    }
    if (reply == CLOUD360_OAUTH_REPLY_DENIED) {
        oauth->status = CLOUD360_OAUTH_DECLINED;
    } else if (reply == CLOUD360_OAUTH_REPLY_EXPIRED) {
        oauth->status = CLOUD360_OAUTH_EXPIRED;
    } else if (reply == CLOUD360_OAUTH_REPLY_ERROR) {
        oauth->status = CLOUD360_OAUTH_FAILED;
    } else {
        if (reply == CLOUD360_OAUTH_REPLY_SLOW_DOWN &&
            oauth->interval_ms <= CLOUD360_OAUTH_INTERVAL_MAX * 1000 - 5000)
            oauth->interval_ms += 5000;
        oauth->next_poll_at = now_ms + oauth->interval_ms;
        oauth->status = CLOUD360_OAUTH_WAITING;
    }
    if (finished(oauth->status))
        clear_secret(&oauth->device, sizeof oauth->device);
}

void cloud360_oauth_cancel(Cloud360OAuth *oauth)
{
    if (!oauth) return;
    clear_secret(&oauth->device, sizeof oauth->device);
    oauth->status = CLOUD360_OAUTH_CANCELLED;
    ++oauth->generation;
}

void cloud360_oauth_logout(Cloud360OAuth *oauth)
{
    if (!oauth) return;
    clear_secret(&oauth->device, sizeof oauth->device);
    oauth->status = CLOUD360_OAUTH_IDLE;
    oauth->attempts = 0;
    ++oauth->generation;
}

void cloud360_oauth_view(const Cloud360OAuth *oauth, uint32_t now_ms,
                          Cloud360OAuthView *view)
{
    if (!view) return;
    memset(view, 0, sizeof *view);
    if (!oauth) return;
    view->status = oauth->status;
    view->attempts = oauth->attempts;
    if (oauth->status != CLOUD360_OAUTH_WAITING &&
        oauth->status != CLOUD360_OAUTH_POLLING)
        return;
    memcpy(view->user_code, oauth->device.user_code,
           sizeof view->user_code);
    memcpy(view->verification_uri, oauth->device.verification_uri,
           sizeof view->verification_uri);
    if ((int32_t)(oauth->expires_at - now_ms) > 0) {
        uint32_t remaining = oauth->expires_at - now_ms;
        view->remaining_seconds = (remaining + 999) / 1000;
    }
    view->interval_seconds = oauth->interval_ms / 1000;
}

const char *cloud360_oauth_status_text(Cloud360OAuthStatus status)
{
    switch (status) {
        case CLOUD360_OAUTH_NOT_CONFIGURED: return "CLIENTE MICROSOFT NAO CONFIGURADO";
        case CLOUD360_OAUTH_IDLE: return "PRONTO PARA AUTORIZAR";
        case CLOUD360_OAUTH_REQUESTING: return "SOLICITANDO CODIGO...";
        case CLOUD360_OAUTH_WAITING: return "AGUARDANDO AUTORIZACAO";
        case CLOUD360_OAUTH_POLLING: return "CONSULTANDO AUTORIZACAO...";
        case CLOUD360_OAUTH_AUTHORIZED: return "OAUTH AUTORIZADO (NAO XBOX LIVE)";
        case CLOUD360_OAUTH_DECLINED: return "AUTORIZACAO RECUSADA";
        case CLOUD360_OAUTH_EXPIRED: return "CODIGO EXPIRADO";
        case CLOUD360_OAUTH_CANCELLED: return "LOGIN CANCELADO";
        case CLOUD360_OAUTH_UNAVAILABLE: return "HTTPS/LOGIN INDISPONIVEL";
        case CLOUD360_OAUTH_FAILED: return "FALHA NA AUTORIZACAO";
        default: return "ESTADO DESCONHECIDO";
    }
}
