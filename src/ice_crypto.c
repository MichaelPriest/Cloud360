#include "ice.h"

/* The Xbox 360 build must supply a vetted cryptographic library.
 * There is intentionally NO insecure homemade HMAC fallback. */
#ifdef CLOUD360_WITH_MBEDTLS
#include <mbedtls/md.h>

bool cloud360_ice_mbedtls_hmac_sha1(
    void *context, const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len, uint8_t digest[20])
{
    (void)context;
    if (!key || !key_len || !data || !digest) return false;
    const mbedtls_md_info_t *info =
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    return info && mbedtls_md_hmac(info, key, key_len,
                                   data, data_len, digest) == 0;
}
#else
bool cloud360_ice_mbedtls_hmac_sha1(
    void *context, const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len, uint8_t digest[20])
{
    (void)context;
    (void)key;
    (void)key_len;
    (void)data;
    (void)data_len;
    (void)digest;
    return false; /* safe until mbedtls-xenon is available */
}
#endif
