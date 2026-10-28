#ifndef CLOUD360_STUN_H
#define CLOUD360_STUN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* STUN RFC 8489 wire format (only IPv4 Binding discovery).
 * This is NOT authenticated ICE connectivity checking and must never
 * signal that a WebRTC connection is ready. No UDP socket is opened.
 * Transaction IDs must come from a cryptographically secure RNG supplied
 * by the network backend; do not use rand(), time() or predictable IDs. */
enum {
    CLOUD360_STUN_HEADER_LEN = 20,
    CLOUD360_STUN_TRANSACTION_LEN = 12,
    CLOUD360_STUN_PACKET_MAX = 1500
};

typedef struct {
    uint8_t ipv4[4];
    uint16_t port;
    bool has_fingerprint; /* validates CRC only, NOT server identity */
} Cloud360StunMapping;

bool cloud360_stun_binding_request(
    const uint8_t transaction_id[CLOUD360_STUN_TRANSACTION_LEN],
    uint8_t out[CLOUD360_STUN_HEADER_LEN]);

/* A valid Binding success response with matching 96-bit transaction ID,
 * magic cookie, and XOR-MAPPED-ADDRESS. Reject malformed TLV, invalid
 * FINGERPRINT and unknown mandatory attributes. No HMAC verifier yet. */
bool cloud360_stun_parse_binding_response(
    const uint8_t *packet, size_t length,
    const uint8_t transaction_id[CLOUD360_STUN_TRANSACTION_LEN],
    Cloud360StunMapping *out);

#endif
