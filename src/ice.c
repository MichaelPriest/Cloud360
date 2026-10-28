#include "ice.h"
#include <string.h>

enum {
    STUN_BINDING_REQUEST = 0x0001,
    STUN_MAGIC_COOKIE = 0x2112A442u,
    ATTR_USERNAME = 0x0006,
    ATTR_MESSAGE_INTEGRITY = 0x0008,
    ATTR_PRIORITY = 0x0024,
    ATTR_USE_CANDIDATE = 0x0025,
    ATTR_ICE_CONTROLLED = 0x8029,
    ATTR_ICE_CONTROLLING = 0x802a,
    ATTR_FINGERPRINT = 0x8028,
    STUN_FINGERPRINT_XOR = 0x5354554eu
};

static void put16(uint8_t *p, uint16_t n)
{
    p[0] = (uint8_t)(n >> 8);
    p[1] = (uint8_t)n;
}
static void put32(uint8_t *p, uint32_t n)
{
    p[0] = (uint8_t)(n >> 24);
    p[1] = (uint8_t)(n >> 16);
    p[2] = (uint8_t)(n >> 8);
    p[3] = (uint8_t)n;
}
static uint32_t fingerprint_crc(const uint8_t *p, size_t bytes)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < bytes; ++i) {
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc ^ STUN_FINGERPRINT_XOR;
}
static size_t bounded(const char *s, size_t cap)
{
    if (!s) return 0;
    size_t len = 0;
    while (len < cap && s[len]) ++len;
    return len;
}
static bool valid_ice_credential(const char *text, size_t max, size_t min)
{
    const size_t len = bounded(text, max + 1u);
    if (len < min || len > max) return false;
    const char *const alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t i = 0; i < len; ++i)
        if (!strchr(alphabet, text[i])) return false;
    return true;
}
static bool append_attr(uint8_t *p, size_t cap, size_t *offset,
                        uint16_t type, const uint8_t *data, size_t len)
{
    const size_t aligned = (len + 3u) & ~(size_t)3u;
    if (len > UINT16_MAX || *offset > cap || cap - *offset < 4u + aligned)
        return false;
    put16(p + *offset, type);
    put16(p + *offset + 2, (uint16_t)len);
    if (data && len) memcpy(p + *offset + 4, data, len);
    if (aligned > len)
        memset(p + *offset + 4 + len, 0, aligned - len);
    *offset += 4u + aligned;
    return true;
}

bool cloud360_ice_build_check(
    const Cloud360IceCheck *check, const uint8_t transaction_id[12],
    Cloud360IceHmacSha1 hmac, void *ctx,
    uint8_t *packet, size_t capacity, size_t *written)
{
    if (written) *written = 0;
    if (!check || !transaction_id || !hmac || !packet || !written ||
        capacity < CLOUD360_STUN_HEADER_LEN || check->priority == 0 ||
        (check->controlling == false && check->nominate) ||
        !valid_ice_credential(check->local_ufrag,CLOUD360_ICE_UFRAG_MAX,4) ||
        !valid_ice_credential(check->remote_ufrag,CLOUD360_ICE_UFRAG_MAX,4) ||
        !valid_ice_credential(check->remote_password,CLOUD360_ICE_PWD_MAX,22))
        return false;
    uint8_t id_nonzero = 0;
    for (size_t i = 0; i < 12; ++i) id_nonzero |= transaction_id[i];
    if (!id_nonzero) return false;
    if (capacity > CLOUD360_STUN_PACKET_MAX)
        capacity = CLOUD360_STUN_PACKET_MAX;

    memset(packet, 0, CLOUD360_STUN_HEADER_LEN);
    put16(packet, STUN_BINDING_REQUEST);
    put32(packet + 4, STUN_MAGIC_COOKIE);
    memcpy(packet + 8, transaction_id, 12);
    size_t offset = CLOUD360_STUN_HEADER_LEN;
    uint8_t username[CLOUD360_ICE_USERNAME_MAX + 1] = { 0 };
    const size_t remote_len = strlen(check->remote_ufrag);
    const size_t local_len = strlen(check->local_ufrag);
    memcpy(username, check->remote_ufrag, remote_len);
    username[remote_len] = ':';
    memcpy(username + remote_len + 1, check->local_ufrag, local_len);
    const size_t username_len = remote_len + 1 + local_len;
    uint8_t priority[4];
    put32(priority, check->priority);
    uint8_t tie[8];
    for (size_t i = 0; i < 8; ++i)
        tie[i] = (uint8_t)(check->tie_breaker >> (56u - (unsigned)i * 8u));

    if (!append_attr(packet, capacity, &offset, ATTR_USERNAME,
                     username, username_len) ||
        !append_attr(packet, capacity, &offset, ATTR_PRIORITY, priority, 4) ||
        !append_attr(packet, capacity, &offset,
                     check->controlling ? ATTR_ICE_CONTROLLING :
                                          ATTR_ICE_CONTROLLED, tie, 8) ||
        (check->nominate &&
         !append_attr(packet, capacity, &offset, ATTR_USE_CANDIDATE, NULL, 0)))
        return false;

    /* RFC 8489 section 14.5: header length reaches the end of MI,
     * while the HMAC input stops BEFORE the MI attribute header. */
    if (capacity - offset < 24u + 8u) return false;
    const size_t integrity_offset = offset;
    put16(packet + 2, (uint16_t)(integrity_offset + 24u - 20u));
    uint8_t digest[CLOUD360_ICE_HMAC_SHA1_LEN] = { 0 };
    const bool ok = hmac(ctx,(const uint8_t *)check->remote_password,
                         strlen(check->remote_password),
                         packet, integrity_offset, digest);
    if (!ok) {
        volatile uint8_t *clear = digest;
        for (size_t i = 0; i < sizeof digest; ++i) clear[i] = 0;
        return false;
    }
    bool appended = append_attr(packet,capacity,&offset,ATTR_MESSAGE_INTEGRITY,
                                digest,sizeof digest);
    volatile uint8_t *clear = digest;
    for (size_t i = 0; i < sizeof digest; ++i) clear[i] = 0;
    if (!appended) return false;

    /* FINGERPRINT is last. Unlike MI, this CRC is NOT authentication. */
    put16(packet + 2, (uint16_t)(offset + 8u - 20u));
    const uint32_t crc = fingerprint_crc(packet, offset);
    uint8_t value[4];
    put32(value, crc);
    if (!append_attr(packet,capacity,&offset,ATTR_FINGERPRINT,value,4))
        return false;
    *written = offset;
    return true;
}

bool cloud360_ice_verify_binding_response(
    const uint8_t *packet, size_t length, const uint8_t transaction_id[12],
    const char *password, Cloud360IceHmacSha1 hmac,
    void *context, Cloud360StunMapping *mapping)
{
    if (!packet || !transaction_id || !password || !hmac || !mapping ||
        !valid_ice_credential(password, CLOUD360_ICE_PWD_MAX,22) ||
        length < 20 || length > CLOUD360_STUN_PACKET_MAX ||
        (length - 20u) % 4u || packet[0] != 0x01 || packet[1] != 0x01 ||
        packet[4] != 0x21 || packet[5] != 0x12 ||
        packet[6] != 0xa4 || packet[7] != 0x42 ||
        (((size_t)packet[2] << 8) | packet[3]) != length - 20u)
        return false;
    uint8_t mismatch = 0;
    for (size_t i = 0; i < 12; ++i)
        mismatch |= packet[8 + i] ^ transaction_id[i];
    if (mismatch) return false;

    Cloud360StunMapping result = { 0 };
    bool mapped = false, integrity = false, fingerprint = false;
    size_t offset = 20;
    while (offset < length) {
        if (length - offset < 4) return false;
        const uint16_t type = (uint16_t)(((uint16_t)packet[offset] << 8) |
                                         packet[offset+1]);
        const uint16_t data_len = (uint16_t)(((uint16_t)packet[offset+2] << 8) |
                                             packet[offset+3]);
        const size_t aligned = ((size_t)data_len + 3u) & ~(size_t)3u;
        if (aligned > length - offset - 4u) return false;
        const uint8_t *value = packet + offset + 4u;
        if (integrity && type != ATTR_FINGERPRINT) return false;

        if (type == 0x0020) { /* XOR-MAPPED-ADDRESS */
            if (mapped || data_len != 8 || value[0] != 0 || value[1] != 1)
                return false;
            const uint16_t port = (uint16_t)(
                (((uint16_t)value[2] << 8) | value[3]) ^ 0x2112u);
            if (!port) return false;
            result.port = port;
            static const uint8_t cookie[4] = {0x21,0x12,0xa4,0x42};
            for (size_t i = 0; i < 4; ++i)
                result.ipv4[i] = value[4+i] ^ cookie[i];
            mapped = true;
        } else if (type == ATTR_MESSAGE_INTEGRITY) {
            if (integrity || data_len != CLOUD360_ICE_HMAC_SHA1_LEN)
                return false;
            uint8_t input[CLOUD360_STUN_PACKET_MAX];
            memcpy(input, packet, offset);
            const size_t end = offset + 4u + CLOUD360_ICE_HMAC_SHA1_LEN;
            input[2] = (uint8_t)((end - 20u) >> 8);
            input[3] = (uint8_t)(end - 20u);
            uint8_t expected[CLOUD360_ICE_HMAC_SHA1_LEN] = { 0 };
            const bool verified = hmac(context,(const uint8_t *)password,
                                       strlen(password),input,offset,expected);
            uint8_t unequal = 0;
            for (size_t i = 0; i < CLOUD360_ICE_HMAC_SHA1_LEN; ++i)
                unequal |= expected[i] ^ value[i];
            volatile uint8_t *wipe = expected;
            for (size_t i = 0; i < sizeof expected; ++i) wipe[i] = 0;
            if (!verified || unequal) return false;
            integrity = true;
        } else if (type == ATTR_FINGERPRINT) {
            if (fingerprint || data_len != 4 || offset + 8u != length)
                return false;
            const uint32_t expected = fingerprint_crc(packet,offset);
            const uint32_t got = ((uint32_t)value[0] << 24) |
                ((uint32_t)value[1] << 16) |
                ((uint32_t)value[2] << 8) | value[3];
            if (got != expected) return false;
            fingerprint = true;
        } else if (type < 0x8000u) {
            /* Unknown comprehension-required attribute: refuse. */
            return false;
        }
        offset += 4u + aligned;
    }
    if (!mapped || !integrity) return false;
    result.has_fingerprint = fingerprint;
    *mapping = result;
    return true;
}
