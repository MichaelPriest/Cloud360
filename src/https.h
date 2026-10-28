#ifndef CLOUD360_HTTPS_H
#define CLOUD360_HTTPS_H

#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>

/* HTTPS-only, bounded and certificate-verified transport.
 * A reachability check does not implement Microsoft login or xCloud. */
enum {
    CLOUD360_HTTPS_HOST_BYTES = 64,
    CLOUD360_HTTPS_PATH_BYTES = 96,
    CLOUD360_HTTPS_CA_BYTES = 96,
    CLOUD360_HTTPS_BODY_LIMIT = 16384,
    CLOUD360_HTTPS_CONNECT_MS = 8000,
    CLOUD360_HTTPS_TOTAL_MS = 15000
};

typedef enum {
    CLOUD360_HTTPS_IDLE = 0,
    CLOUD360_HTTPS_RUNNING,
    CLOUD360_HTTPS_OK,
    CLOUD360_HTTPS_BAD_REQUEST,
    CLOUD360_HTTPS_UNAVAILABLE,
    CLOUD360_HTTPS_CA_MISSING,
    CLOUD360_HTTPS_TLS_ERROR,
    CLOUD360_HTTPS_TIMEOUT,
    CLOUD360_HTTPS_TOO_LARGE,
    CLOUD360_HTTPS_HTTP_ERROR,
    CLOUD360_HTTPS_TRANSPORT_ERROR
} Cloud360HttpsStatus;

typedef struct {
    char host[CLOUD360_HTTPS_HOST_BYTES];
    char path[CLOUD360_HTTPS_PATH_BYTES];
    char ca_file[CLOUD360_HTTPS_CA_BYTES];
} Cloud360HttpsRequest;

typedef struct {
    Cloud360HttpsStatus status;
    long http_status;
    size_t bytes_received;
    int transport_error;
    uint32_t attempts;
    bool busy;
} Cloud360HttpsSnapshot;

/* A transport handles one request and writes bounded diagnostics.
 * It MUST verify the certificate chain and hostname, enforce HTTPS-only,
 * and fail closed; the transport is never allowed to disable verification. */
typedef Cloud360HttpsStatus (*Cloud360HttpsFetch)(
    void *context, const Cloud360HttpsRequest *request,
    long *http_code, size_t *bytes, int *transport_error);

typedef struct {
    Cloud360HttpsFetch fetch;
    void *context;
} Cloud360HttpsTransport;

typedef struct {
    pthread_mutex_t mutex;
    bool initialized;
    Cloud360HttpsSnapshot result;
    Cloud360HttpsRequest request;
    Cloud360HttpsTransport transport;
} Cloud360Https;

int cloud360_https_init(Cloud360Https *client);
bool cloud360_https_request_valid(const Cloud360HttpsRequest *request);
bool cloud360_https_start(Cloud360Https *client,
                          const Cloud360HttpsTransport *transport,
                          const Cloud360HttpsRequest *request);
void cloud360_https_snapshot(Cloud360Https *client, Cloud360HttpsSnapshot *result);
const char *cloud360_https_status_text(Cloud360HttpsStatus status);

/* XSDK curl-xenon transport; returns UNAVAILABLE unless built with
 * CLOUD360_WITH_CURL and linked against curl-xenon + mbedtls-xenon. */
/* Optional bounded response body. NULL discards bytes; non-NULL stores at
 * most capacity-1 bytes and terminates with NUL on success.
 * The whole call fails on overflow; no truncated JSON is accepted. */
typedef struct {
    char *data;
    size_t capacity;
    size_t used;
} Cloud360HttpsBody;

/* POST application/x-www-form-urlencoded; exposes bounded JSON body even
 * on HTTP 4xx OAuth responses, but NEVER on TLS/certificate failures.
 * TLS settings are identical to the GET backend. No redirects. */
Cloud360HttpsStatus cloud360_https_curl_post_json(
    void *context, const Cloud360HttpsRequest *request,
    const char *json, Cloud360HttpsBody *body,
    long *http_code, size_t *bytes, int *transport_error);

Cloud360HttpsStatus cloud360_https_curl_post_form(
    void *context, const Cloud360HttpsRequest *request,
    const char *encoded_form, Cloud360HttpsBody *body,
    long *http_code, size_t *bytes, int *transport_error);

Cloud360HttpsStatus cloud360_https_curl_get(
    void *context, const Cloud360HttpsRequest *request,
    Cloud360HttpsBody *body, long *http_code, size_t *bytes, int *transport_error);

Cloud360HttpsStatus cloud360_https_curl_fetch(
    void *context, const Cloud360HttpsRequest *request,
    long *http_code, size_t *bytes, int *transport_error);

#endif
