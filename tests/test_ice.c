#include "../src/ice.h"

#include <openssl/hmac.h>
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint8_t txid[12] = {
    0xb7,0xe7,0xa7,0x01,0xbc,0x34,0xd6,0x86,0xfa,0x87,0xdf,0xae
};
static const char remote_pwd[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
static const Cloud360IceCheck check = {
    .local_ufrag = "local01",
    .remote_ufrag = "remote02",
    .remote_password = remote_pwd,
    .priority = 1845494271u,
    .tie_breaker = UINT64_C(0x1234567890abcdef),
    .controlling = true,
    .nominate = false
};
static bool openssl_hmac(void *context,
                         const uint8_t *key, size_t key_len,
                         const uint8_t *data, size_t len,
                         uint8_t digest[20])
{
    (void)context;
    if (key_len > INT_MAX) return false;
    unsigned produced = 0;
    return HMAC(EVP_sha1(), key, (int)key_len, data, len,
                digest, &produced) && produced == 20;
}
static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static uint32_t crc32_with_xor(const uint8_t *p, size_t len)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc ^ 0x5354554eu;
}
static void put16(uint8_t *p, uint16_t n)
{
    p[0] = (uint8_t)(n >> 8); p[1] = (uint8_t)n;
}
static void put32(uint8_t *p, uint32_t n)
{
    p[0] = (uint8_t)(n >> 24); p[1] = (uint8_t)(n >> 16);
    p[2] = (uint8_t)(n >> 8); p[3] = (uint8_t)n;
}
static size_t find_attr(const uint8_t *packet, size_t len, uint16_t type)
{
    size_t at = 20;
    while (at + 4 <= len) {
        uint16_t bytes = be16(packet + at + 2);
        if (at + 4u + bytes > len) return 0;
        if (be16(packet + at) == type) return at;
        at += 4u + ((bytes + 3u) & ~(size_t)3u);
    }
    return 0;
}

static bool unavailable_hmac(void *ctx,const uint8_t *key,size_t kn,
                             const uint8_t *data,size_t dn,uint8_t out[20])
{
    (void)ctx; (void)key; (void)kn; (void)data; (void)dn; (void)out;
    return false;
}

static void test_request(void)
{
    uint8_t packet[1500] = {0};
    size_t len = 0;
    assert(!cloud360_ice_build_check(&check, txid,
            unavailable_hmac, NULL, packet, sizeof packet, &len));
    assert(!cloud360_ice_build_check(&check, txid,
            cloud360_ice_mbedtls_hmac_sha1, NULL,
            packet, sizeof packet, &len)); /* default fails closed */
    assert(!cloud360_ice_build_check(&check, txid,
            NULL, NULL, packet, sizeof packet, &len));
    assert(!cloud360_ice_build_check(&check, txid,
            openssl_hmac, NULL, packet, 48, &len));
    assert(cloud360_ice_build_check(&check, txid,
            openssl_hmac, NULL, packet, sizeof packet, &len));
    assert(len >= 88 && len < 1500);
    assert(be16(packet) == 1 && be16(packet + 2) == len - 20);
    assert(memcmp(packet + 8, txid, 12) == 0);

    const size_t username = find_attr(packet,len,0x0006);
    const size_t priority = find_attr(packet,len,0x0024);
    const size_t role = find_attr(packet,len,0x802a);
    const size_t integrity = find_attr(packet,len,0x0008);
    const size_t fingerprint = find_attr(packet,len,0x8028);
    assert(username && priority && role && integrity && fingerprint);
    assert(be16(packet + username + 2) == strlen("remote02:local01"));
    assert(!memcmp(packet + username + 4,"remote02:local01",strlen("remote02:local01")));
    assert(be16(packet + integrity + 2) == 20);
    assert(fingerprint + 8 == len);
    /* Compare against independently computed OpenSSL HMAC. */
    uint8_t digest[20] = {0};
    const uint16_t full_body = be16(packet + 2);
    put16(packet + 2, (uint16_t)(integrity + 24 - 20));
    assert(openssl_hmac(NULL,(const uint8_t *)remote_pwd,strlen(remote_pwd),
                        packet, integrity, digest));
    assert(!memcmp(digest, packet + integrity + 4,20));
    put16(packet + 2, full_body);
    const uint32_t crc = crc32_with_xor(packet, fingerprint);
    const uint32_t stored = ((uint32_t)packet[fingerprint+4]<<24) |
        ((uint32_t)packet[fingerprint+5]<<16) |
        ((uint32_t)packet[fingerprint+6]<<8) | packet[fingerprint+7];
    assert(stored == crc);

    Cloud360IceCheck modified = check;
    modified.nominate = true;
    assert(cloud360_ice_build_check(&modified,txid,openssl_hmac,NULL,
                                    packet,sizeof packet,&len));
    assert(find_attr(packet,len,0x0025) != 0);
    modified.controlling = false;
    assert(!cloud360_ice_build_check(&modified,txid,openssl_hmac,NULL,
                                     packet,sizeof packet,&len));
    modified.nominate = false;
    assert(cloud360_ice_build_check(&modified,txid,openssl_hmac,NULL,
                                    packet,sizeof packet,&len));
    assert(find_attr(packet,len,0x8029) != 0);
    modified.remote_ufrag = "invalid:username";
    assert(!cloud360_ice_build_check(&modified,txid,openssl_hmac,NULL,
                                     packet,sizeof packet,&len));
}

static size_t build_success(uint8_t *packet, size_t cap, bool with_fingerprint)
{
    assert(cap >= 60);
    memset(packet, 0, cap);
    put16(packet, 0x0101);
    put32(packet + 4, 0x2112a442);
    memcpy(packet + 8,txid,12);
    /* XOR-MAPPED-ADDRESS 192.0.2.1:32853 */
    static const uint8_t mapped[] = {
        0x00,0x20,0x00,0x08,0x00,0x01,0xa1,0x47,
        0xe1,0x12,0xa6,0x43
    };
    memcpy(packet + 20,mapped,sizeof mapped);
    const size_t mi_at = 32;
    put16(packet + mi_at,0x0008);
    put16(packet + mi_at + 2,20);
    put16(packet + 2,(uint16_t)(mi_at + 24 - 20));
    uint8_t digest[20];
    assert(openssl_hmac(NULL,(const uint8_t *)remote_pwd,strlen(remote_pwd),
                        packet,mi_at,digest));
    memcpy(packet + mi_at + 4,digest,20);
    size_t len = 56;
    if(with_fingerprint){
        len = 64;
        put16(packet + 2,(uint16_t)(len - 20));
        put16(packet + 56,0x8028);
        put16(packet + 58,4);
        put32(packet + 60,crc32_with_xor(packet,56));
    }
    return len;
}

static void test_response(void)
{
    uint8_t packet[100];
    size_t len = build_success(packet,sizeof packet,true);
    Cloud360StunMapping map = { 0 };
    assert(cloud360_ice_verify_binding_response(packet,len,txid,remote_pwd,
                                                 openssl_hmac,NULL,&map));
    assert(map.port==32853 && map.ipv4[0]==192 && map.ipv4[3]==1);
    assert(map.has_fingerprint);
    assert(!cloud360_stun_parse_binding_response(packet,len,txid,&map));
    /* The unauthenticated STUN decoder still rejects MESSAGE-INTEGRITY. */
    assert(!cloud360_ice_verify_binding_response(packet,len,txid,
           "OTHERpasswordABCDEFGHIJKL",openssl_hmac,NULL,&map));
    assert(!cloud360_ice_verify_binding_response(packet,len,txid,remote_pwd,
           unavailable_hmac,NULL,&map));

    uint8_t altered[100];
    memcpy(altered,packet,len);
    altered[31] ^= 1; /* address corruption invalidates HMAC */
    assert(!cloud360_ice_verify_binding_response(altered,len,txid,remote_pwd,
                                                  openssl_hmac,NULL,&map));
    memcpy(altered,packet,len);
    altered[52] ^= 1; /* digest corruption */
    assert(!cloud360_ice_verify_binding_response(altered,len,txid,remote_pwd,
                                                  openssl_hmac,NULL,&map));
    memcpy(altered,packet,len);
    altered[63] ^= 1; /* fingerprint corruption */
    assert(!cloud360_ice_verify_binding_response(altered,len,txid,remote_pwd,
                                                  openssl_hmac,NULL,&map));
    memcpy(altered,packet,len);
    altered[8] ^= 1; /* transaction mismatch */
    assert(!cloud360_ice_verify_binding_response(altered,len,txid,remote_pwd,
                                                  openssl_hmac,NULL,&map));

    len=build_success(packet,sizeof packet,false);
    assert(cloud360_ice_verify_binding_response(packet,len,txid,remote_pwd,
                                                openssl_hmac,NULL,&map));
    assert(!map.has_fingerprint);
    assert(!cloud360_ice_verify_binding_response(packet,32,txid,remote_pwd,
                                                  openssl_hmac,NULL,&map));
    assert(!cloud360_ice_verify_binding_response(packet,len,txid,remote_pwd,
                                                  NULL,NULL,&map));
}

int main(void)
{
    test_request();
    test_response();
    puts("Cloud360 authenticated ICE STUN HMAC-SHA1 requests/responses: PASS");
    return 0;
}
