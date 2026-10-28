#include "stun.h"

#include <string.h>

enum {
    STUN_BINDING_REQUEST = 0x0001,
    STUN_BINDING_SUCCESS = 0x0101,
    STUN_COOKIE = 0x2112A442u,
    STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020,
    STUN_ATTR_FINGERPRINT = 0x8028,
    STUN_ATTR_SOFTWARE = 0x8022,
    STUN_ATTR_MAPPED_ADDRESS = 0x0001,
    STUN_ATTR_RESPONSE_ORIGIN = 0x802b,
    STUN_ATTR_OTHER_ADDRESS = 0x802c
};

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}
static void put32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

/* RFC 5389/8489 CRC-32 (polynomial 0xEDB88320), then XOR 0x5354554E.
 * A STUN fingerprint is a corruption check, not authentication. */
static uint32_t crc32_stun(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc ^ 0x5354554eu;
}

bool cloud360_stun_binding_request(const uint8_t transaction_id[12],
                                   uint8_t out[20])
{
    if (!transaction_id || !out) return false;
    uint8_t nonzero = 0;
    for (size_t i = 0; i < CLOUD360_STUN_TRANSACTION_LEN; ++i)
        nonzero |= transaction_id[i];
    if (!nonzero) return false;
    memset(out, 0, CLOUD360_STUN_HEADER_LEN);
    put16(out, STUN_BINDING_REQUEST);
    put16(out + 2, 0);
    put32(out + 4, STUN_COOKIE);
    memcpy(out + 8, transaction_id, CLOUD360_STUN_TRANSACTION_LEN);
    return true;
}

bool cloud360_stun_parse_binding_response(
    const uint8_t *packet, size_t length,
    const uint8_t transaction_id[12], Cloud360StunMapping *out)
{
    if (!packet || !transaction_id || !out ||
        length < CLOUD360_STUN_HEADER_LEN ||
        length > CLOUD360_STUN_PACKET_MAX ||
        (length - CLOUD360_STUN_HEADER_LEN) % 4 != 0)
        return false;
    if (be16(packet) != STUN_BINDING_SUCCESS ||
        be32(packet + 4) != STUN_COOKIE ||
        be16(packet + 2) != length - CLOUD360_STUN_HEADER_LEN)
        return false;

    uint8_t mismatch = 0;
    for (size_t i = 0; i < 12; ++i)
        mismatch |= packet[8 + i] ^ transaction_id[i];
    if (mismatch) return false;

    Cloud360StunMapping result = { 0 };
    bool mapped = false;
    size_t cursor = CLOUD360_STUN_HEADER_LEN;
    while (cursor < length) {
        if (length - cursor < 4) return false;
        const uint16_t type = be16(packet + cursor);
        const uint16_t bytes = be16(packet + cursor + 2);
        const size_t aligned = ((size_t)bytes + 3u) & ~(size_t)3u;
        if (aligned > length - cursor - 4u) return false;
        const uint8_t *value = packet + cursor + 4;

        if (type == STUN_ATTR_XOR_MAPPED_ADDRESS) {
            if (mapped || bytes != 8 || value[0] != 0 ||
                value[1] != 0x01) return false;
            const uint16_t port = (uint16_t)(be16(value + 2) ^ 0x2112u);
            if (port == 0) return false;
            result.port = port;
            static const uint8_t cookie_bytes[4] = { 0x21, 0x12, 0xA4, 0x42 };
            for (size_t i = 0; i < 4; ++i)
                result.ipv4[i] = value[i + 4] ^ cookie_bytes[i];
            mapped = true;
        } else if (type == STUN_ATTR_FINGERPRINT) {
            /* Fingerprint is required to be last when present. */
            if (result.has_fingerprint || bytes != 4 ||
                cursor + 8 != length ||
                be32(value) != crc32_stun(packet, cursor))
                return false;
            result.has_fingerprint = true;
        } else if (type == STUN_ATTR_MAPPED_ADDRESS) {
            /* Compatibility attribute is not a substitute for the XOR form. */
            if (bytes != 8 && bytes != 20) return false;
        } else if (type == STUN_ATTR_SOFTWARE ||
                   type == STUN_ATTR_RESPONSE_ORIGIN ||
                   type == STUN_ATTR_OTHER_ADDRESS) {
            /* Only informational optional attributes are recognized. */
        } else if (type < 0x8000u) {
            /* Explicitly fail closed on MESSAGE-INTEGRITY, ice checks,
             * and unknown comprehension-required attributes. */
            return false;
        }
        cursor += 4u + aligned;
    }
    if (!mapped) return false;
    *out = result;
    return true;
}
