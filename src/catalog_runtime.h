#ifndef CLOUD360_CATALOG_RUNTIME_H
#define CLOUD360_CATALOG_RUNTIME_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "catalog.h"
#include "https.h"

enum {
    CLOUD360_CATALOG_VISIBLE = 3
};

typedef enum {
    CLOUD360_CATALOG_EMPTY = 0,
    CLOUD360_CATALOG_LOCAL_READY,
    CLOUD360_CATALOG_FILE_MISSING,
    CLOUD360_CATALOG_INVALID_JSON,
    CLOUD360_CATALOG_NO_SOURCE,
    CLOUD360_CATALOG_LOADING,
    CLOUD360_CATALOG_ONLINE_READY,
    CLOUD360_CATALOG_HTTPS_FAILED,
    CLOUD360_CATALOG_WORKER_FAILED
} Cloud360CatalogState;

typedef Cloud360HttpsStatus (*Cloud360CatalogFetch)(
    void *context, const Cloud360HttpsRequest *request,
    Cloud360HttpsBody *body, long *http_code,
    size_t *bytes, int *transport_error);

typedef struct {
    Cloud360CatalogState state;
    Cloud360HttpsStatus https_status;
    int transport_error;
    long http_code;
    bool busy;
    bool loaded;
    uint32_t count;
    uint32_t first_index;
    uint32_t selected_index;
    uint32_t visible_count;
    Cloud360Game visible[CLOUD360_CATALOG_VISIBLE];
} Cloud360CatalogView;

/* Keep this object alive for the lifetime of the title: its worker detaches.
 * Only the runtime owns staging and the download buffer. */
typedef struct {
    pthread_mutex_t mutex;
    bool initialized;
    bool busy;
    Cloud360CatalogState state;
    Cloud360HttpsStatus https_status;
    int transport_error;
    long http_code;
    Cloud360Catalog active;
    Cloud360Catalog staging;
    char payload[CLOUD360_CATALOG_JSON_MAX + 1];
    Cloud360HttpsRequest request;
    Cloud360CatalogFetch fetch;
    void *fetch_context;
} Cloud360CatalogRuntime;

int cloud360_catalog_runtime_init(Cloud360CatalogRuntime *runtime,
                                  Cloud360CatalogFetch fetch,
                                  void *fetch_context);

/* Safe to call on the UI thread. Only 16 KiB is read. Never replaces
 * previously loaded catalogue on a missing or invalid file. */
bool cloud360_catalog_runtime_local(Cloud360CatalogRuntime *runtime,
                                    const char *file);

/* HTTPS only. Caller supplies trusted host/path/CA. Invalid request fails
 * before a network worker is started. */
bool cloud360_catalog_runtime_start(Cloud360CatalogRuntime *runtime,
                                    const Cloud360HttpsRequest *request);

/* Config file: two whitespace-separated tokens, host and absolute path.
 * Missing file means no provider configured, not Xbox/Game Pass access. */
bool cloud360_catalog_runtime_source(Cloud360CatalogRuntime *runtime,
                                     const char *source_file,
                                     const char *ca_file);

void cloud360_catalog_runtime_snapshot(Cloud360CatalogRuntime *runtime,
                                       uint32_t selected_index,
                                       Cloud360CatalogView *view);

const char *cloud360_catalog_state_text(Cloud360CatalogState status);

#endif
