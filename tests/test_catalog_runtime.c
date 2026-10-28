#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../src/catalog_runtime.h"

static Cloud360CatalogRuntime runtime;

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool gate;
    bool entered;
    Cloud360HttpsStatus result;
    const char *json;
    unsigned calls;
} Mock;

static Cloud360HttpsStatus fetch_mock(
    void *ctx, const Cloud360HttpsRequest *request,
    Cloud360HttpsBody *body, long *status, size_t *bytes, int *err)
{
    Mock *m = ctx;
    assert(strcmp(request->host, "example.com") == 0);
    assert(strcmp(request->path, "/catalog.json") == 0);
    assert(strcmp(request->ca_file, "game:/cacert.pem") == 0);

    (void)pthread_mutex_lock(&m->lock);
    m->entered = true;
    m->calls++;
    (void)pthread_cond_broadcast(&m->cond);
    while (m->gate)
        (void)pthread_cond_wait(&m->cond, &m->lock);
    const Cloud360HttpsStatus result = m->result;
    const char *json = m->json;
    (void)pthread_mutex_unlock(&m->lock);

    *status = result == CLOUD360_HTTPS_OK ? 200 : 0;
    *err = result == CLOUD360_HTTPS_OK ? 0 : EHOSTUNREACH;
    *bytes = 0;
    body->used = 0;
    if (result != CLOUD360_HTTPS_OK)
        return result;

    const size_t len = strlen(json);
    assert(len < body->capacity);
    memcpy(body->data, json, len + 1);
    body->used = *bytes = len;
    return CLOUD360_HTTPS_OK;
}

static Cloud360CatalogView wait_result(void)
{
    const struct timespec delay = { 0, 5000000 };
    Cloud360CatalogView view;
    for (unsigned i = 0; i < 200; ++i) {
        cloud360_catalog_runtime_snapshot(&runtime, 0, &view);
        if (!view.busy) return view;
        (void)nanosleep(&delay, NULL);
    }
    assert(0 && "Catalog worker never finished");
    return (Cloud360CatalogView){ 0 };
}

int main(void)
{
    static Mock m = {
        .gate = true,
        .result = CLOUD360_HTTPS_OK,
        .json = "{\"games\":[{\"id\":\"on\",\"title\":\"Online example\"}]}"
    };
    assert(pthread_mutex_init(&m.lock, NULL) == 0);
    assert(pthread_cond_init(&m.cond, NULL) == 0);
    assert(cloud360_catalog_runtime_init(&runtime, fetch_mock, &m) == 0);

    Cloud360CatalogView view;
    cloud360_catalog_runtime_snapshot(&runtime, 0, &view);
    assert(!view.loaded && !view.busy && view.count == 0);
    assert(!cloud360_catalog_runtime_local(&runtime,
                                          "/tmp/does-not-exist-cloud360.json"));
    cloud360_catalog_runtime_snapshot(&runtime, 0, &view);
    assert(view.state == CLOUD360_CATALOG_FILE_MISSING);

    FILE *f = fopen("/tmp/catalog-local-test.json", "wb");
    assert(f);
    const char *local = "{\"games\":[{\"id\":\"l\",\"title\":\"Local example\"},"
                        "{\"id\":\"m\",\"title\":\"Another game\"},"
                        "{\"id\":\"n\",\"title\":\"Third entry\"},"
                        "{\"id\":\"o\",\"title\":\"Fourth entry\"}]}";
    assert(fwrite(local, 1, strlen(local), f) == strlen(local));
    assert(fclose(f) == 0);
    assert(cloud360_catalog_runtime_local(&runtime,
                                         "/tmp/catalog-local-test.json"));
    cloud360_catalog_runtime_snapshot(&runtime, 3, &view);
    assert(view.loaded && view.count == 4);
    assert(view.first_index == 3 && view.visible_count == 1);
    assert(strcmp(view.visible[0].title, "Fourth entry") == 0);

    Cloud360HttpsRequest request = {
        .host = "example.com",
        .path = "/catalog.json",
        .ca_file = "game:/cacert.pem"
    };
    assert(cloud360_catalog_runtime_start(&runtime, &request));
    (void)pthread_mutex_lock(&m.lock);
    while (!m.entered)
        (void)pthread_cond_wait(&m.cond, &m.lock);
    (void)pthread_mutex_unlock(&m.lock);
    cloud360_catalog_runtime_snapshot(&runtime, 0, &view);
    assert(view.busy && view.state == CLOUD360_CATALOG_LOADING);
    assert(view.count == 4);
    assert(!cloud360_catalog_runtime_start(&runtime, &request));
    assert(!cloud360_catalog_runtime_local(&runtime,
                                          "/tmp/catalog-local-test.json"));

    (void)pthread_mutex_lock(&m.lock);
    m.gate = false;
    (void)pthread_cond_broadcast(&m.cond);
    (void)pthread_mutex_unlock(&m.lock);
    view = wait_result();
    assert(view.state == CLOUD360_CATALOG_ONLINE_READY);
    assert(view.count == 1);
    assert(strcmp(view.visible[0].title, "Online example") == 0);

    m.json = "{\"games\":[{\"id\":\"invalid\",\"title\":";
    assert(cloud360_catalog_runtime_start(&runtime, &request));
    view = wait_result();
    assert(view.state == CLOUD360_CATALOG_INVALID_JSON && view.count == 1);
    assert(strcmp(view.visible[0].id, "on") == 0);

    m.result = CLOUD360_HTTPS_TLS_ERROR;
    assert(cloud360_catalog_runtime_start(&runtime, &request));
    view = wait_result();
    assert(view.state == CLOUD360_CATALOG_HTTPS_FAILED);
    assert(view.https_status == CLOUD360_HTTPS_TLS_ERROR);
    assert(view.count == 1);

    assert(!cloud360_catalog_runtime_source(&runtime,
                                            "/tmp/notconfigured-cloud360.txt",
                                            "game:/cacert.pem"));
    cloud360_catalog_runtime_snapshot(&runtime, 0, &view);
    assert(view.state == CLOUD360_CATALOG_NO_SOURCE && view.count == 1);

    f = fopen("/tmp/catalog-source-test.txt", "wb");
    assert(f);
    const char *source = "example.com /catalog.json\n";
    assert(fwrite(source, 1, strlen(source), f) == strlen(source));
    assert(fclose(f) == 0);
    m.result = CLOUD360_HTTPS_OK;
    m.json = "{\"games\":[]}";
    assert(cloud360_catalog_runtime_source(&runtime,
                "/tmp/catalog-source-test.txt", "game:/cacert.pem"));
    view = wait_result();
    assert(view.state == CLOUD360_CATALOG_ONLINE_READY && view.loaded);
    assert(view.count == 0);

    assert(remove("/tmp/catalog-local-test.json") == 0);
    assert(remove("/tmp/catalog-source-test.txt") == 0);
    assert(m.calls == 4);
    assert(pthread_cond_destroy(&m.cond) == 0);
    assert(pthread_mutex_destroy(&m.lock) == 0);
    puts("Cloud360 local/remote catalog state and safe async refresh: PASS");
    return 0;
}
