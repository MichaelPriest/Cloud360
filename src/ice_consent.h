#ifndef CLOUD360_ICE_CONSENT_H
#define CLOUD360_ICE_CONSENT_H

#include "ice_agent.h"
#include "ice.h"
#include "udp.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * RFC 7675-style consent freshness for ONE externally selected,
 * nominated IPv4/UDP candidate pair. This is a conservative transport
 * component, NOT a WebRTC implementation or an authorization to play.
 *
 * No media is sent by this module. Future DTLS/RTP senders MUST call
 * can_send() on the same monotonic clock before EVERY media send.
 * No default random number generator or insecure HMAC fallback exists.
 */
enum {
    CLOUD360_CONSENT_EXPIRY_MS = 30000,
    CLOUD360_CONSENT_INTERVAL_MIN_MS = 4000,
    CLOUD360_CONSENT_INTERVAL_MAX_MS = 6000,
    CLOUD360_CONSENT_TRANSACTIONS = 8
};

typedef enum {
    CLOUD360_CONSENT_IDLE = 0,
    CLOUD360_CONSENT_PROBING,
    CLOUD360_CONSENT_GRANTED,
    CLOUD360_CONSENT_EXPIRED,
    CLOUD360_CONSENT_REVOKED,
    CLOUD360_CONSENT_CANCELLED,
    CLOUD360_CONSENT_CRYPTO_UNAVAILABLE
} Cloud360ConsentState;

typedef struct {
    Cloud360UdpPeer nominated_remote;
    const char *local_ufrag;
    const char *remote_ufrag;
    const char *remote_password;
    /* Must come from a future COMPLETE ICE nomination/checklist,
     * not just the first authenticated STUN Binding response. */
    bool selected_pair_confirmed;
} Cloud360ConsentConfig;

typedef struct {
    uint8_t transaction_id[CLOUD360_STUN_TRANSACTION_LEN];
    uint32_t sent_at_ms;
    bool used;
    bool pending;
} Cloud360ConsentTransaction;

typedef struct {
    Cloud360ConsentState state;
    Cloud360UdpPeer remote;
    char local_ufrag[CLOUD360_ICE_UFRAG_MAX + 1];
    char remote_ufrag[CLOUD360_ICE_UFRAG_MAX + 1];
    char remote_password[CLOUD360_ICE_PWD_MAX + 1];
    Cloud360ConsentTransaction transactions[CLOUD360_CONSENT_TRANSACTIONS];
    uint32_t started_at_ms;
    uint32_t last_consent_at_ms;
    uint32_t next_probe_ms;
    uint32_t generation;
    uint32_t probes_sent;
    uint32_t verified_responses;
    uint32_t rejected_responses;
    Cloud360IceHmacSha1 hmac;
    Cloud360IceRandom random;
    Cloud360IceSend send;
    void *context;
} Cloud360IceConsent;

typedef struct {
    Cloud360ConsentState state;
    uint32_t generation;
    uint32_t probes_sent;
    uint32_t verified_responses;
    uint32_t rejected_responses;
    uint32_t age_seconds;
    bool can_send_media;
    /* Deliberately no credentials, remote addresses, or transaction IDs. */
} Cloud360ConsentView;

void cloud360_consent_init(Cloud360IceConsent *consent);
bool cloud360_consent_start(
    Cloud360IceConsent *consent, const Cloud360ConsentConfig *config,
    Cloud360IceHmacSha1 hmac, Cloud360IceRandom random,
    Cloud360IceSend send, void *context, uint32_t now_ms);

/* May emit ONE consent Binding request per tick. Each request uses
 * fresh CSPRNG ID, is sent ONCE, and the next delay is 4-6 seconds.
 * On failure or lost consent, credentials are erased and send is blocked. */
bool cloud360_consent_tick(Cloud360IceConsent *consent, uint32_t now_ms);

/* Only a matching, authenticated, non-error response from the exact
 * selected transport address renews consent. Replays are rejected. */
bool cloud360_consent_receive(
    Cloud360IceConsent *consent, uint32_t generation,
    Cloud360UdpPeer source, const uint8_t *packet, size_t bytes,
    uint32_t now_ms);

/* Media senders should re-evaluate this at every send, not cache it. */
bool cloud360_consent_can_send(const Cloud360IceConsent *consent,
                               uint32_t now_ms);

void cloud360_consent_revoke(Cloud360IceConsent *consent);
void cloud360_consent_cancel(Cloud360IceConsent *consent);
void cloud360_consent_view(const Cloud360IceConsent *consent,
                            uint32_t now_ms, Cloud360ConsentView *view);
const char *cloud360_consent_status(Cloud360ConsentState state);

#endif
