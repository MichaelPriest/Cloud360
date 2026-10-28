#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../src/https.h"

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool gated;
    bool entered;
    Cloud360HttpsStatus answer;
    long http_code;
    size_t bytes;
    int backend_error;
    int calls;
} Mock;

static Cloud360HttpsStatus fake_transport(
    void *context, const Cloud360HttpsRequest *request,
    long *http_status, size_t *bytes, int *error)
{
    Mock *mock = context;
    assert(strcmp(request->host, "example.com") == 0);
    assert(strcmp(request->path, "/") == 0);
    assert(strcmp(request->ca_file, "game:/cacert.pem") == 0);
    (void)pthread_mutex_lock(&mock->mutex);
    ++mock->calls;
    mock->entered = true;
    (void)pthread_cond_broadcast(&mock->condition);
    while (mock->gated)
        (void)pthread_cond_wait(&mock->condition, &mock->mutex);
    *http_status = mock->http_code;
    *bytes = mock->bytes;
    *error = mock->backend_error;
    const Cloud360HttpsStatus result = mock->answer;
    (void)pthread_mutex_unlock(&mock->mutex);
    return result;
}

static Cloud360HttpsSnapshot await_done(Cloud360Https *client)
{
    const struct timespec pause = { .tv_sec = 0, .tv_nsec = 5000000 };
    for (int i = 0; i < 200; ++i) {
        Cloud360HttpsSnapshot result;
        cloud360_https_snapshot(client, &result);
        if (!result.busy)
            return result;
        (void)nanosleep(&pause, NULL);
    }
    assert(0 && "HTTPS async test timeout");
    return (Cloud360HttpsSnapshot){ 0 };
}

static void test_validation(void)
{
    Cloud360HttpsRequest r = {
        .host = "example.com", .path = "/path?item=one&n=2",
        .ca_file = "game:/cacert.pem"
    };
    assert(cloud360_https_request_valid(&r));
    strcpy(r.host, "https://example.com");
    assert(!cloud360_https_request_valid(&r));
    strcpy(r.host, "example.com");
    strcpy(r.path, "//bad");
    assert(!cloud360_https_request_valid(&r));
    strcpy(r.path, "/bad\r\nHeader:injected");
    assert(!cloud360_https_request_valid(&r));
    strcpy(r.path, "/");
    r.host[0] = '-';
    assert(!cloud360_https_request_valid(&r));
    strcpy(r.host, "example.com");
    r.ca_file[0] = 0;
    assert(!cloud360_https_request_valid(&r));
    assert(!cloud360_https_request_valid(NULL));
}

int main(void)
{
    test_validation();
    assert(cloud360_https_init(NULL) == EINVAL);
    static Cloud360Https client;
    assert(cloud360_https_init(&client) == 0);
    Cloud360HttpsRequest request = {
        .host = "example.com", .path = "/", .ca_file = "game:/cacert.pem"
    };
    Mock mock = {
        .gated = true, .answer = CLOUD360_HTTPS_OK, .http_code = 200,
        .bytes = 512
    };
    assert(pthread_mutex_init(&mock.mutex, NULL) == 0);
    assert(pthread_cond_init(&mock.condition, NULL) == 0);
    Cloud360HttpsTransport transport = { fake_transport, &mock };
    Cloud360HttpsSnapshot result;
    cloud360_https_snapshot(&client, &result);
    assert(result.status == CLOUD360_HTTPS_IDLE);
    assert(!cloud360_https_start(&client, NULL, &request));

    assert(cloud360_https_start(&client, &transport, &request));
    (void)pthread_mutex_lock(&mock.mutex);
    while (!mock.entered)
        (void)pthread_cond_wait(&mock.condition, &mock.mutex);
    (void)pthread_mutex_unlock(&mock.mutex);
    cloud360_https_snapshot(&client, &result);
    assert(result.status == CLOUD360_HTTPS_RUNNING && result.busy);
    assert(!cloud360_https_start(&client, &transport, &request));
    (void)pthread_mutex_lock(&mock.mutex);
    mock.gated = false;
    (void)pthread_cond_broadcast(&mock.condition);
    (void)pthread_mutex_unlock(&mock.mutex);

    result = await_done(&client);
    assert(result.status == CLOUD360_HTTPS_OK &&
           result.http_status == 200 && result.bytes_received == 512 &&
           result.attempts == 1);

    mock.http_code = 302;
    assert(cloud360_https_start(&client, &transport, &request));
    result = await_done(&client);
    assert(result.status == CLOUD360_HTTPS_HTTP_ERROR);

    mock.http_code = 200;
    mock.bytes = CLOUD360_HTTPS_BODY_LIMIT + 1;
    assert(cloud360_https_start(&client, &transport, &request));
    result = await_done(&client);
    assert(result.status == CLOUD360_HTTPS_TOO_LARGE);

    mock.answer = CLOUD360_HTTPS_TLS_ERROR;
    mock.bytes = 0;
    mock.backend_error = 60;
    assert(cloud360_https_start(&client, &transport, &request));
    result = await_done(&client);
    assert(result.status == CLOUD360_HTTPS_TLS_ERROR &&
           result.transport_error == 60 && result.attempts == 4);
    assert(!strcmp(cloud360_https_status_text(result.status),
                   "FALHA TLS/CERTIFICADO"));

    assert(mock.calls == 4);
    assert(pthread_cond_destroy(&mock.condition) == 0);
    assert(pthread_mutex_destroy(&mock.mutex) == 0);
    puts("Cloud360 bounded HTTPS request, async state and TLS failure: PASS");
    return 0;
}
