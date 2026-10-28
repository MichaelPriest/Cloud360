/* HTTPS GET adapter for curl-xenon + mbedtls-xenon.
 * Without CLOUD360_WITH_CURL the function returns UNAVAILABLE; it NEVER
 * silently falls back to plain HTTP or unverified TLS. */
#include "https.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef CLOUD360_WITH_CURL
#include <curl/curl.h>

typedef struct {
    size_t received;
    bool too_large;
    Cloud360HttpsBody *body;
} Cloud360WriteState;

static size_t receive_body(char *data, size_t size, size_t nmemb, void *opaque)
{
    Cloud360WriteState *state = opaque;
    if (size != 0 && nmemb > SIZE_MAX / size) {
        state->too_large = true;
        return 0;
    }
    const size_t amount = size * nmemb;
    if (amount > CLOUD360_HTTPS_BODY_LIMIT - state->received) {
        state->too_large = true;
        return 0;
    }
    if (state->body) {
        Cloud360HttpsBody *body = state->body;
        if (!body->data || !body->capacity ||
            amount > body->capacity - 1 - body->used) {
            state->too_large = true;
            return 0;
        }
        memcpy(body->data + body->used, data, amount);
        body->used += amount;
        body->data[body->used] = '\0';
    }
    state->received += amount;
    return amount;
}

static pthread_once_t curl_once = PTHREAD_ONCE_INIT;
static CURLcode curl_init_status = CURLE_FAILED_INIT;
static void init_curl_once(void)
{
    curl_init_status = curl_global_init(CURL_GLOBAL_DEFAULT);
}

/* Every setopt is checked. A missing security option fails closed. */
#define REQUIRE_OPT(handle, key, value) do { \
    if (curl_easy_setopt((handle), (key), (value)) != CURLE_OK) { \
        result = CLOUD360_HTTPS_UNAVAILABLE; \
        goto end; \
    } \
} while (0)

static Cloud360HttpsStatus curl_execute(
    void *context, const Cloud360HttpsRequest *request, const char *payload, bool json_post,
    Cloud360HttpsBody *response, long *http_code,
    size_t *bytes, int *transport_error)
{
    (void)context;
    if (!http_code || !bytes || !transport_error ||
        !cloud360_https_request_valid(request))
        return CLOUD360_HTTPS_BAD_REQUEST;

    *http_code = 0;
    *bytes = 0;
    *transport_error = 0;
    if (payload) {
        const size_t n = strlen(payload);
        if (!n || n > 10000) return CLOUD360_HTTPS_BAD_REQUEST;
        if (json_post) {
            if (payload[0] != '{' || payload[n - 1] != '}')
                return CLOUD360_HTTPS_BAD_REQUEST;
            for (size_t i=0; i<n; ++i) {
                const unsigned char ch=(unsigned char)payload[i];
                if (ch < 32 || ch > 126) return CLOUD360_HTTPS_BAD_REQUEST;
            }
        } else {
            for (size_t i=0; i<n; ++i) {
                unsigned char ch=(unsigned char)payload[i];
                const bool ordinary=(ch>='A'&&ch<='Z')||
                    (ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||
                    ch=='-'||ch=='_'||ch=='.'||ch=='~'||
                    ch=='='||ch=='&'||ch=='+';
                if(ch=='%'){
                    if(i+2>=n) return CLOUD360_HTTPS_BAD_REQUEST;
                    for(size_t k=1;k<=2;++k){
                        ch=(unsigned char)payload[i+k];
                        if(!((ch>='0'&&ch<='9')||(ch>='A'&&ch<='F')||
                             (ch>='a'&&ch<='f')))return CLOUD360_HTTPS_BAD_REQUEST;
                    }
                    i+=2;
                }else if(!ordinary)return CLOUD360_HTTPS_BAD_REQUEST;
            }
        }
    }
    if (response) {
        if (!response->data || response->capacity < 2 ||
            response->capacity > CLOUD360_HTTPS_BODY_LIMIT + 1)
            return CLOUD360_HTTPS_BAD_REQUEST;
        response->used = 0;
        response->data[0] = '\0';
    }

    /* The title provides its own Mozilla root bundle beside the XEX.
     * Never disable certificate checks when missing or invalid. */
    FILE *ca = fopen(request->ca_file, "rb");
    if (!ca)
        return CLOUD360_HTTPS_CA_MISSING;
    const int ca_ok = fgetc(ca);
    (void)fclose(ca);
    if (ca_ok == EOF)
        return CLOUD360_HTTPS_CA_MISSING;

    if (pthread_once(&curl_once, init_curl_once) != 0 ||
        curl_init_status != CURLE_OK)
        return CLOUD360_HTTPS_UNAVAILABLE;

    char url[CLOUD360_HTTPS_HOST_BYTES + CLOUD360_HTTPS_PATH_BYTES + 10];
    const int url_chars = snprintf(url, sizeof url, "https://%s%s",
                                   request->host, request->path);
    if (url_chars <= 0 || (size_t)url_chars >= sizeof url)
        return CLOUD360_HTTPS_BAD_REQUEST;

    CURL *handle = curl_easy_init();
    if (!handle)
        return CLOUD360_HTTPS_UNAVAILABLE;
    Cloud360HttpsStatus result = CLOUD360_HTTPS_TRANSPORT_ERROR;
    Cloud360WriteState body = { .body = response };
    CURLcode code = CURLE_FAILED_INIT;
    struct curl_slist *headers = NULL;

    REQUIRE_OPT(handle, CURLOPT_URL, url);
    if (payload) {
        headers = curl_slist_append(NULL, json_post ?
            "Content-Type: application/json" :
            "Content-Type: application/x-www-form-urlencoded");
        if (!headers) {
            result = CLOUD360_HTTPS_UNAVAILABLE;
            goto end;
        }
        if (json_post) {
            struct curl_slist *extended = curl_slist_append(
                headers, "x-xbl-contract-version: 1");
            if (!extended) {
                result = CLOUD360_HTTPS_UNAVAILABLE;
                goto end;
            }
            headers = extended;
        }
        REQUIRE_OPT(handle, CURLOPT_HTTPHEADER, headers);
        REQUIRE_OPT(handle, CURLOPT_POST, 1L);
        REQUIRE_OPT(handle, CURLOPT_POSTFIELDS, payload);
        REQUIRE_OPT(handle, CURLOPT_POSTFIELDSIZE, (long)strlen(payload));
    } else {
        REQUIRE_OPT(handle, CURLOPT_HTTPGET, 1L);
    }
    REQUIRE_OPT(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    REQUIRE_OPT(handle, CURLOPT_PROTOCOLS_STR, "https");
    REQUIRE_OPT(handle, CURLOPT_FOLLOWLOCATION, 0L);
    REQUIRE_OPT(handle, CURLOPT_MAXREDIRS, 0L);
    REQUIRE_OPT(handle, CURLOPT_SSL_VERIFYPEER, 1L);
    REQUIRE_OPT(handle, CURLOPT_SSL_VERIFYHOST, 2L);
    REQUIRE_OPT(handle, CURLOPT_SSLVERSION,
                (long)(CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_TLSv1_2));
    REQUIRE_OPT(handle, CURLOPT_CAINFO, request->ca_file);
    REQUIRE_OPT(handle, CURLOPT_CONNECTTIMEOUT_MS,
                (long)CLOUD360_HTTPS_CONNECT_MS);
    REQUIRE_OPT(handle, CURLOPT_TIMEOUT_MS, (long)CLOUD360_HTTPS_TOTAL_MS);
    REQUIRE_OPT(handle, CURLOPT_LOW_SPEED_LIMIT, 1L);
    REQUIRE_OPT(handle, CURLOPT_LOW_SPEED_TIME, 8L);
    REQUIRE_OPT(handle, CURLOPT_NOSIGNAL, 1L);
    REQUIRE_OPT(handle, CURLOPT_PROXY, "");
    REQUIRE_OPT(handle, CURLOPT_NETRC, CURL_NETRC_IGNORED);
    REQUIRE_OPT(handle, CURLOPT_USERAGENT, "Cloud360/0.3 (Xbox360; XSDK)");
    REQUIRE_OPT(handle, CURLOPT_MAXFILESIZE_LARGE,
                (curl_off_t)CLOUD360_HTTPS_BODY_LIMIT);
    REQUIRE_OPT(handle, CURLOPT_WRITEFUNCTION, receive_body);
    REQUIRE_OPT(handle, CURLOPT_WRITEDATA, &body);

    code = curl_easy_perform(handle);
    (void)curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, http_code);
    *bytes = body.received;
    *transport_error = (int)code;

    if (body.too_large || code == CURLE_FILESIZE_EXCEEDED)
        result = CLOUD360_HTTPS_TOO_LARGE;
    else if (code == CURLE_OPERATION_TIMEDOUT)
        result = CLOUD360_HTTPS_TIMEOUT;
    else if (code == CURLE_PEER_FAILED_VERIFICATION ||
             code == CURLE_SSL_CONNECT_ERROR ||
             code == CURLE_SSL_CERTPROBLEM ||
             code == CURLE_SSL_ISSUER_ERROR)
        result = CLOUD360_HTTPS_TLS_ERROR;
    else if (code != CURLE_OK)
        result = CLOUD360_HTTPS_TRANSPORT_ERROR;
    else if (*http_code < 200 || *http_code >= 300)
        result = CLOUD360_HTTPS_HTTP_ERROR;
    else
        result = CLOUD360_HTTPS_OK;

end:
    curl_easy_cleanup(handle);
    curl_slist_free_all(headers);
    /* OAuth errors are valid HTTPS responses and must be parsed on 4xx.
     * Never expose data if TLS verification, transport or limits fail. */
    if (response && result != CLOUD360_HTTPS_OK &&
        !(payload && result == CLOUD360_HTTPS_HTTP_ERROR)) {
        response->used = 0;
        response->data[0] = '\0';
    }
    return result;
}

Cloud360HttpsStatus cloud360_https_curl_get(
    void *context, const Cloud360HttpsRequest *request,
    Cloud360HttpsBody *body, long *http_code, size_t *bytes,
    int *transport_error)
{
    return curl_execute(context, request, NULL, false, body, http_code, bytes,
                        transport_error);
}

Cloud360HttpsStatus cloud360_https_curl_post_form(
    void *context, const Cloud360HttpsRequest *request,
    const char *form, Cloud360HttpsBody *body, long *http_code,
    size_t *bytes, int *transport_error)
{
    if (!form) return CLOUD360_HTTPS_BAD_REQUEST;
    return curl_execute(context, request, form, false, body, http_code, bytes,
                        transport_error);
}

Cloud360HttpsStatus cloud360_https_curl_post_json(
    void *context, const Cloud360HttpsRequest *request,
    const char *json, Cloud360HttpsBody *body, long *http_code,
    size_t *bytes, int *transport_error)
{
    if (!json) return CLOUD360_HTTPS_BAD_REQUEST;
    return curl_execute(context, request, json, true, body, http_code,
                        bytes, transport_error);
}

Cloud360HttpsStatus cloud360_https_curl_fetch(
    void *context, const Cloud360HttpsRequest *request,
    long *http_code, size_t *bytes, int *transport_error)
{
    return cloud360_https_curl_get(
        context, request, NULL, http_code, bytes, transport_error);
}
#undef REQUIRE_OPT

#else

Cloud360HttpsStatus cloud360_https_curl_post_json(
    void *context, const Cloud360HttpsRequest *request,
    const char *json, Cloud360HttpsBody *body, long *http_code,
    size_t *bytes, int *transport_error)
{
    if (!json) return CLOUD360_HTTPS_BAD_REQUEST;
    return cloud360_https_curl_get(context, request, body, http_code,
                                   bytes, transport_error);
}

Cloud360HttpsStatus cloud360_https_curl_post_form(
    void *context, const Cloud360HttpsRequest *request,
    const char *form, Cloud360HttpsBody *body, long *http_code,
    size_t *bytes, int *transport_error)
{
    (void)form;
    return cloud360_https_curl_get(context, request, body, http_code,
                                   bytes, transport_error);
}

Cloud360HttpsStatus cloud360_https_curl_get(
    void *context, const Cloud360HttpsRequest *request,
    Cloud360HttpsBody *body, long *http_code, size_t *bytes,
    int *transport_error)
{
    (void)context;
    if (body) {
        if (!body->data || body->capacity == 0)
            return CLOUD360_HTTPS_BAD_REQUEST;
        body->data[0] = '\0';
        body->used = 0;
    }
    if (http_code) *http_code = 0;
    if (bytes) *bytes = 0;
    if (transport_error) *transport_error = ENOSYS;
    return cloud360_https_request_valid(request) ?
           CLOUD360_HTTPS_UNAVAILABLE : CLOUD360_HTTPS_BAD_REQUEST;
}

Cloud360HttpsStatus cloud360_https_curl_fetch(
    void *context, const Cloud360HttpsRequest *request,
    long *http_code, size_t *bytes, int *transport_error)
{
    return cloud360_https_curl_get(
        context, request, NULL, http_code, bytes, transport_error);
}

#endif
