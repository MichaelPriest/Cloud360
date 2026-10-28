#include "../src/stun.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t txid[12] = {
    0xb7,0xe7,0xa7,0x01,0xbc,0x34,0xd6,0x86,0xfa,0x87,0xdf,0xae
};

/* Fixed RFC-like response: XOR-mapped address 192.0.2.1, port 32853.
 * CRC32 XOR 0x5354554e is a known value for these exact 32 bytes. */
static const uint8_t valid_reply[40] = {
    0x01,0x01,0x00,0x14,
    0x21,0x12,0xa4,0x42,
    0xb7,0xe7,0xa7,0x01,0xbc,0x34,0xd6,0x86,
    0xfa,0x87,0xdf,0xae,
    0x00,0x20,0x00,0x08,
    0x00,0x01,0xa1,0x47,0xe1,0x12,0xa6,0x43,
    0x80,0x28,0x00,0x04,0x7d,0x28,0x1f,0x59
};

static void test_request(void)
{
    uint8_t request[CLOUD360_STUN_HEADER_LEN] = { 0 };
    assert(cloud360_stun_binding_request(txid, request));
    assert(request[0] == 0 && request[1] == 1);
    assert(request[2] == 0 && request[3] == 0);
    assert(request[4] == 0x21 && request[7] == 0x42);
    assert(memcmp(request + 8, txid, 12) == 0);
    const uint8_t empty[12] = { 0 };
    assert(!cloud360_stun_binding_request(empty, request));
    assert(!cloud360_stun_binding_request(NULL, request));
    assert(!cloud360_stun_binding_request(txid, NULL));
}

static void test_response(void)
{
    Cloud360StunMapping mapped = { 0 };
    assert(cloud360_stun_parse_binding_response(
        valid_reply, sizeof valid_reply, txid, &mapped));
    assert(mapped.ipv4[0] == 192);
    assert(mapped.ipv4[1] == 0);
    assert(mapped.ipv4[2] == 2);
    assert(mapped.ipv4[3] == 1);
    assert(mapped.port == 32853);
    assert(mapped.has_fingerprint);

    uint8_t corrupted[sizeof valid_reply];
    memcpy(corrupted, valid_reply, sizeof corrupted);
    corrupted[25] ^= 1;
    assert(!cloud360_stun_parse_binding_response(
        corrupted, sizeof corrupted, txid, &mapped));
    memcpy(corrupted, valid_reply, sizeof corrupted);
    corrupted[39] ^= 1;
    assert(!cloud360_stun_parse_binding_response(
        corrupted, sizeof corrupted, txid, &mapped));
    memcpy(corrupted, valid_reply, sizeof corrupted);
    corrupted[0] = 0x00; /* Wrong response type */
    assert(!cloud360_stun_parse_binding_response(
        corrupted, sizeof corrupted, txid, &mapped));
    memcpy(corrupted, valid_reply, sizeof corrupted);
    corrupted[4] ^= 1; /* Wrong magic cookie */
    assert(!cloud360_stun_parse_binding_response(
        corrupted, sizeof corrupted, txid, &mapped));
    memcpy(corrupted, valid_reply, sizeof corrupted);
    corrupted[8] ^= 1; /* Wrong transaction identifier */
    assert(!cloud360_stun_parse_binding_response(
        corrupted, sizeof corrupted, txid, &mapped));
    memcpy(corrupted, valid_reply, sizeof corrupted);
    corrupted[2] = 0; corrupted[3] = 0x18; /* TLV byte mismatch */
    assert(!cloud360_stun_parse_binding_response(
        corrupted, sizeof corrupted, txid, &mapped));
    memcpy(corrupted, valid_reply, sizeof corrupted);
    corrupted[22] = 0xFF; /* Attribute length too large */
    assert(!cloud360_stun_parse_binding_response(
        corrupted, sizeof corrupted, txid, &mapped));
    memcpy(corrupted, valid_reply, sizeof corrupted);
    corrupted[20] = 0x00; corrupted[21] = 0x08; /* MESSAGE-INTEGRITY
                                                    not yet supported. */
    assert(!cloud360_stun_parse_binding_response(
        corrupted, sizeof corrupted, txid, &mapped));

    memcpy(corrupted, valid_reply, sizeof corrupted);
    corrupted[25] = 0x02; /* IPv6 unsupported */
    assert(!cloud360_stun_parse_binding_response(
        corrupted, sizeof corrupted, txid, &mapped));

    assert(!cloud360_stun_parse_binding_response(
        valid_reply, sizeof valid_reply - 1, txid, &mapped));
    assert(!cloud360_stun_parse_binding_response(
        valid_reply, 19, txid, &mapped));
    assert(!cloud360_stun_parse_binding_response(
        NULL, sizeof valid_reply, txid, &mapped));
    assert(!cloud360_stun_parse_binding_response(
        valid_reply, sizeof valid_reply, NULL, &mapped));
    assert(!cloud360_stun_parse_binding_response(
        valid_reply, sizeof valid_reply, txid, NULL));
}

static void test_optional_fingerprint(void)
{
    /* With no FINGERPRINT, the result is an untrusted/unauthenticated
     * diagnostic address; it NEVER establishes ICE. */
    uint8_t packet[32];
    memcpy(packet, valid_reply, 32);
    packet[2] = 0; packet[3] = 12;
    Cloud360StunMapping mapped = { 0 };
    assert(cloud360_stun_parse_binding_response(
        packet, sizeof packet, txid, &mapped));
    assert(mapped.port == 32853 && !mapped.has_fingerprint);

    packet[20] = 0x80; packet[21] = 0x22; /* SOFTWARE optional
                                           but mapped address absent */
    assert(!cloud360_stun_parse_binding_response(
        packet, sizeof packet, txid, &mapped));
}

int main(void)
{
    test_request();
    test_response();
    test_optional_fingerprint();
    puts("Cloud360 STUN RFC8489 binding, XOR address and CRC: PASS");
    return 0;
}
