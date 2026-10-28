#ifndef CLOUD360_ICE_H
#define CLOUD360_ICE_H

#include "stun.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bounded subset of RFC 8445 connectivity checks (IPv4, UDP).
 * Not a full ICE agent: no candidate checklists, retransmissions,
 * role conflict handling, TURN, nomination policy or consent freshness.
 * A valid STUN response must NOT be treated as connected WebRTC.
 *
 * HMAC is injected through a cryptographic backend; never substitute
 * a predictable hash or skip verification on production paths. */
enum {
    CLOUD360_ICE_UFRAG_MAX = 256,
    CLOUD360_ICE_PWD_MAX = 256,
    CLOUD360_ICE_USERNAME_MAX = CLOUD360_ICE_UFRAG_MAX * 2 + 1,
    CLOUD360_ICE_HMAC_SHA1_LEN = 20
};

typedef bool (*Cloud360IceHmacSha1)(
    void *context, const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len,
    uint8_t digest[CLOUD360_ICE_HMAC_SHA1_LEN]);

typedef struct {
    const char *local_ufrag;
    const char *remote_ufrag;
    const char *remote_password;
    uint32_t priority;
    uint64_t tie_breaker;
    bool controlling;
    bool nominate;
} Cloud360IceCheck;

/* The transaction ID MUST come from a CSPRNG; this function never
 * supplies random numbers or opens network connections. Adds a STUN
 * FINGERPRINT after MESSAGE-INTEGRITY. Returns false if no crypto. */
bool cloud360_ice_build_check(
    const Cloud360IceCheck *check,
    const uint8_t transaction_id[CLOUD360_STUN_TRANSACTION_LEN],
    Cloud360IceHmacSha1 hmac, void *crypto_context,
    uint8_t *packet, size_t capacity, size_t *written);

/* RFC 7675 consent freshness sends a Binding Request with a NEW
 * cryptographically random transaction ID for every attempt. This is
 * distinct from an ICE connectivity check and does not nominate peers. */
bool cloud360_ice_build_consent_request(
    const char *local_ufrag, const char *remote_ufrag,
    const char *remote_password,
    const uint8_t transaction_id[CLOUD360_STUN_TRANSACTION_LEN],
    Cloud360IceHmacSha1 hmac, void *crypto_context,
    uint8_t *packet, size_t capacity, size_t *written);

/* Response authenticated with the REMOTE ICE password, matching the
 * same transaction ID. Requires MESSAGE-INTEGRITY (HMAC-SHA1).
 * A valid response is not ICE nomination, consent or DTLS trust. */
bool cloud360_ice_verify_binding_response(
    const uint8_t *packet, size_t length,
    const uint8_t transaction_id[CLOUD360_STUN_TRANSACTION_LEN],
    const char *remote_password,
    Cloud360IceHmacSha1 hmac, void *crypto_context,
    Cloud360StunMapping *mapping);

/* Optional mbedTLS backend, disabled unless explicitly integrated. */
bool cloud360_ice_mbedtls_hmac_sha1(
    void *context, const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len,
    uint8_t digest[CLOUD360_ICE_HMAC_SHA1_LEN]);

#endif
