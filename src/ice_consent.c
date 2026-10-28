#include "ice_consent.h"
#include <string.h>

static void wipe(void *data, size_t n)
{
    volatile unsigned char *p = data;
    while (n--) *p++ = 0;
}
static size_t bounded(const char *s, size_t cap)
{
    size_t len = 0;
    if (!s) return 0;
    while (len < cap && s[len]) ++len;
    return len;
}
static bool credential(const char *s, size_t min, size_t max)
{
    const size_t n = bounded(s, max + 1);
    if (n < min || n > max) return false;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '+' || c == '/'))
            return false;
    }
    return true;
}
static bool valid_peer(Cloud360UdpPeer peer)
{
    return peer.port != 0 && peer.ip[0] != 0 &&
           peer.ip[0] < 224 &&
           !(peer.ip[0] == 169 && peer.ip[1] == 254);
}
static bool peer_equal(Cloud360UdpPeer a, Cloud360UdpPeer b)
{
    return a.port == b.port && memcmp(a.ip, b.ip, 4) == 0;
}
static bool age_at_least(uint32_t now, uint32_t then, uint32_t ms)
{
    return (uint32_t)(now - then) >= ms;
}
static bool due(uint32_t now, uint32_t when)
{
    return (int32_t)(now - when) >= 0;
}
static bool active(Cloud360ConsentState s)
{
    return s == CLOUD360_CONSENT_PROBING ||
           s == CLOUD360_CONSENT_GRANTED;
}
static void clear_secure(Cloud360IceConsent *c)
{
    wipe(c->local_ufrag, sizeof c->local_ufrag);
    wipe(c->remote_ufrag, sizeof c->remote_ufrag);
    wipe(c->remote_password, sizeof c->remote_password);
    wipe(c->transactions, sizeof c->transactions);
    c->hmac = NULL;
    c->random = NULL;
    c->send = NULL;
    c->context = NULL;
}
static void stop(Cloud360IceConsent *c, Cloud360ConsentState state)
{
    clear_secure(c);
    c->state = state;
    ++c->generation;
}
void cloud360_consent_init(Cloud360IceConsent *c)
{
    if (c) memset(c, 0, sizeof *c);
}
bool cloud360_consent_start(Cloud360IceConsent *c,
                             const Cloud360ConsentConfig *cfg,
                             Cloud360IceHmacSha1 hmac,
                             Cloud360IceRandom random,
                             Cloud360IceSend send, void *context,
                             uint32_t now)
{
    if (!c || !cfg || c->state != CLOUD360_CONSENT_IDLE ||
        !cfg->selected_pair_confirmed ||
        !valid_peer(cfg->nominated_remote) ||
        !credential(cfg->local_ufrag, 4, CLOUD360_ICE_UFRAG_MAX) ||
        !credential(cfg->remote_ufrag, 4, CLOUD360_ICE_UFRAG_MAX) ||
        !credential(cfg->remote_password, 22, CLOUD360_ICE_PWD_MAX))
        return false;
    if (!hmac || !random || !send) {
        stop(c, CLOUD360_CONSENT_CRYPTO_UNAVAILABLE);
        return false;
    }

    c->remote = cfg->nominated_remote;
    memcpy(c->local_ufrag,cfg->local_ufrag,strlen(cfg->local_ufrag)+1);
    memcpy(c->remote_ufrag,cfg->remote_ufrag,strlen(cfg->remote_ufrag)+1);
    memcpy(c->remote_password,cfg->remote_password,
           strlen(cfg->remote_password)+1);
    c->hmac = hmac;
    c->random = random;
    c->send = send;
    c->context = context;
    c->state = CLOUD360_CONSENT_PROBING;
    c->started_at_ms = now;
    c->last_consent_at_ms = now;
    c->next_probe_ms = now;
    c->probes_sent = 0;
    c->verified_responses = 0;
    c->rejected_responses = 0;
    memset(c->transactions, 0, sizeof c->transactions);
    ++c->generation;
    return true;
}
static bool expired(const Cloud360IceConsent *c, uint32_t now)
{
    return age_at_least(now, c->last_consent_at_ms,
                        CLOUD360_CONSENT_EXPIRY_MS);
}
static void expire_slots(Cloud360IceConsent *c, uint32_t now)
{
    for (size_t i = 0; i < CLOUD360_CONSENT_TRANSACTIONS; ++i) {
        Cloud360ConsentTransaction *t = &c->transactions[i];
        if (t->used && age_at_least(now, t->sent_at_ms,
                                   CLOUD360_CONSENT_EXPIRY_MS))
            wipe(t, sizeof *t);
    }
}
bool cloud360_consent_tick(Cloud360IceConsent *c, uint32_t now)
{
    if (!c || !active(c->state)) return false;
    if (expired(c, now)) {
        stop(c, CLOUD360_CONSENT_EXPIRED);
        return false;
    }
    expire_slots(c, now);
    if (!due(now, c->next_probe_ms)) return false;
    size_t free_index = CLOUD360_CONSENT_TRANSACTIONS;
    for (size_t i = 0; i < CLOUD360_CONSENT_TRANSACTIONS; ++i) {
        if (!c->transactions[i].used) { free_index = i; break; }
    }
    if (free_index == CLOUD360_CONSENT_TRANSACTIONS) {
        stop(c, CLOUD360_CONSENT_CRYPTO_UNAVAILABLE);
        return false;
    }

    /* 96-bit CSPRNG transaction ID + independent randomized pacing.
     * One call to a vetted random provider, never a guessed fallback. */
    uint8_t random_bytes[14] = { 0 };
    if (!c->random(c->context, random_bytes, sizeof random_bytes)) {
        stop(c, CLOUD360_CONSENT_CRYPTO_UNAVAILABLE);
        return false;
    }
    uint8_t nonzero = 0;
    for (size_t i = 0; i < 12; ++i) nonzero |= random_bytes[i];
    if (!nonzero) {
        wipe(random_bytes, sizeof random_bytes);
        stop(c, CLOUD360_CONSENT_CRYPTO_UNAVAILABLE);
        return false;
    }
    for (size_t i = 0; i < CLOUD360_CONSENT_TRANSACTIONS; ++i) {
        if (c->transactions[i].used &&
            memcmp(c->transactions[i].transaction_id, random_bytes, 12) == 0) {
            wipe(random_bytes, sizeof random_bytes);
            stop(c, CLOUD360_CONSENT_CRYPTO_UNAVAILABLE);
            return false;
        }
    }

    uint8_t packet[CLOUD360_STUN_PACKET_MAX] = { 0 };
    size_t length = 0;
    const bool built = cloud360_ice_build_consent_request(
        c->local_ufrag,c->remote_ufrag,c->remote_password,
        random_bytes,c->hmac,c->context,packet,sizeof packet,&length);
    const bool sent = built && length &&
        c->send(c->context,&c->remote,packet,length);
    wipe(packet,sizeof packet);
    if (!sent) {
        wipe(random_bytes, sizeof random_bytes);
        stop(c, CLOUD360_CONSENT_CRYPTO_UNAVAILABLE);
        return false;
    }

    Cloud360ConsentTransaction *slot = &c->transactions[free_index];
    memcpy(slot->transaction_id, random_bytes, 12);
    slot->sent_at_ms = now;
    slot->used = true;
    slot->pending = true;
    const uint32_t uniform =
        ((uint32_t)random_bytes[12] << 8) | random_bytes[13];
    const uint32_t interval = CLOUD360_CONSENT_INTERVAL_MIN_MS +
        (uniform * (CLOUD360_CONSENT_INTERVAL_MAX_MS -
                    CLOUD360_CONSENT_INTERVAL_MIN_MS)) / 65535u;
    c->next_probe_ms = now + interval;
    ++c->probes_sent;
    wipe(random_bytes, sizeof random_bytes);
    return true;
}
bool cloud360_consent_receive(Cloud360IceConsent *c, uint32_t generation,
                               Cloud360UdpPeer peer,
                               const uint8_t *packet, size_t bytes,
                               uint32_t now)
{
    if (!c || !active(c->state) || generation != c->generation ||
        !packet || bytes < 20 || bytes > CLOUD360_STUN_PACKET_MAX)
        return false;
    if (expired(c, now)) {
        stop(c, CLOUD360_CONSENT_EXPIRED);
        return false;
    }
    expire_slots(c, now);
    if (!peer_equal(peer,c->remote)) {
        ++c->rejected_responses;
        return false;
    }
    for (size_t i = 0; i < CLOUD360_CONSENT_TRANSACTIONS; ++i) {
        Cloud360ConsentTransaction *slot = &c->transactions[i];
        if (!slot->used || !slot->pending ||
            memcmp(packet + 8, slot->transaction_id, 12) != 0)
            continue;

        Cloud360StunMapping mapping;
        if (!cloud360_ice_verify_binding_response(
            packet,bytes,slot->transaction_id,c->remote_password,
            c->hmac,c->context,&mapping)) {
            ++c->rejected_responses;
            return false;
        }
        slot->pending = false; /* replay cannot renew consent */
        c->last_consent_at_ms = now;
        c->state = CLOUD360_CONSENT_GRANTED;
        ++c->verified_responses;
        return true;
    }
    ++c->rejected_responses;
    return false;
}
bool cloud360_consent_can_send(const Cloud360IceConsent *c, uint32_t now)
{
    return c && c->state == CLOUD360_CONSENT_GRANTED &&
           !expired(c, now);
}
void cloud360_consent_revoke(Cloud360IceConsent *c)
{
    if (c) stop(c,CLOUD360_CONSENT_REVOKED);
}
void cloud360_consent_cancel(Cloud360IceConsent *c)
{
    if (c) stop(c,CLOUD360_CONSENT_CANCELLED);
}
void cloud360_consent_view(const Cloud360IceConsent *c, uint32_t now,
                           Cloud360ConsentView *v)
{
    if (!v) return;
    memset(v,0,sizeof *v);
    if (!c) return;
    v->state = c->state;
    v->generation = c->generation;
    v->probes_sent = c->probes_sent;
    v->verified_responses = c->verified_responses;
    v->rejected_responses = c->rejected_responses;
    v->can_send_media = cloud360_consent_can_send(c,now);
    if (active(c->state))
        v->age_seconds = (uint32_t)(now - c->last_consent_at_ms)/1000u;
}
const char *cloud360_consent_status(Cloud360ConsentState state)
{
    switch (state) {
        case CLOUD360_CONSENT_IDLE: return "CONSENTIMENTO NAO INICIADO";
        case CLOUD360_CONSENT_PROBING: return "CONSENTIMENTO AINDA NAO CONCEDIDO";
        case CLOUD360_CONSENT_GRANTED: return "CONSENTIMENTO ATIVO";
        case CLOUD360_CONSENT_EXPIRED: return "CONSENTIMENTO EXPIRADO";
        case CLOUD360_CONSENT_REVOKED: return "CONSENTIMENTO REVOGADO";
        case CLOUD360_CONSENT_CANCELLED: return "CONSENTIMENTO CANCELADO";
        case CLOUD360_CONSENT_CRYPTO_UNAVAILABLE:
            return "CRIPTOGRAFIA/TRANSPORTE INDISPONIVEL";
        default: return "ESTADO DESCONHECIDO";
    }
}
