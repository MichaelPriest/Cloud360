#include "catalog_runtime.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static void *remote_worker(void *opaque)
{
    Cloud360CatalogRuntime *runtime = opaque;
    Cloud360HttpsBody body = {
        .data = runtime->payload, .capacity = sizeof runtime->payload,
        .used = 0
    };
    long status = 0;
    size_t bytes = 0;
    int error = 0;
    const Cloud360HttpsStatus result = runtime->fetch(
        runtime->fetch_context, &runtime->request,
        &body, &status, &bytes, &error);
    const bool parsed = result == CLOUD360_HTTPS_OK &&
                        body.used == bytes &&
                        cloud360_catalog_parse(&runtime->staging,
                                               body.data, body.used);

    (void)pthread_mutex_lock(&runtime->mutex);
    runtime->https_status = result;
    runtime->http_code = status;
    runtime->transport_error = error;
    if (parsed) {
        runtime->active = runtime->staging;
        runtime->state = CLOUD360_CATALOG_ONLINE_READY;
    } else {
        runtime->state = result == CLOUD360_HTTPS_OK ?
                         CLOUD360_CATALOG_INVALID_JSON :
                         CLOUD360_CATALOG_HTTPS_FAILED;
    }
    runtime->busy = false;
    (void)pthread_mutex_unlock(&runtime->mutex);
    return NULL;
}

int cloud360_catalog_runtime_init(Cloud360CatalogRuntime *runtime,
                                  Cloud360CatalogFetch fetch,
                                  void *context)
{
    if (!runtime || !fetch)
        return EINVAL;
    memset(runtime, 0, sizeof *runtime);
    const int error = pthread_mutex_init(&runtime->mutex, NULL);
    if (error)
        return error;
    runtime->initialized = true;
    runtime->fetch = fetch;
    runtime->fetch_context = context;
    runtime->state = CLOUD360_CATALOG_EMPTY;
    runtime->https_status = CLOUD360_HTTPS_IDLE;
    return 0;
}

bool cloud360_catalog_runtime_local(Cloud360CatalogRuntime *runtime,
                                    const char *file)
{
    if (!runtime || !runtime->initialized || !file)
        return false;
    /* Mark the operation busy under one lock to exclude remote workers. */
    (void)pthread_mutex_lock(&runtime->mutex);
    if (runtime->busy) {
        (void)pthread_mutex_unlock(&runtime->mutex);
        return false;
    }
    runtime->busy = true;
    (void)pthread_mutex_unlock(&runtime->mutex);

    FILE *f = fopen(file, "rb");
    const bool exists = f != NULL;
    if (f) (void)fclose(f);
    const bool ok = exists &&
        cloud360_catalog_load_file(&runtime->staging, file);

    (void)pthread_mutex_lock(&runtime->mutex);
    if (ok) runtime->active = runtime->staging;
    runtime->state = ok ? CLOUD360_CATALOG_LOCAL_READY :
                     exists ? CLOUD360_CATALOG_INVALID_JSON :
                              CLOUD360_CATALOG_FILE_MISSING;
    runtime->busy = false;
    (void)pthread_mutex_unlock(&runtime->mutex);
    return ok;
}

bool cloud360_catalog_runtime_start(Cloud360CatalogRuntime *runtime,
                                    const Cloud360HttpsRequest *request)
{
    if (!runtime || !runtime->initialized ||
        !cloud360_https_request_valid(request))
        return false;

    (void)pthread_mutex_lock(&runtime->mutex);
    if (runtime->busy) {
        (void)pthread_mutex_unlock(&runtime->mutex);
        return false;
    }

    runtime->request = *request;
    runtime->busy = true;
    runtime->state = CLOUD360_CATALOG_LOADING;
    runtime->https_status = CLOUD360_HTTPS_RUNNING;
    runtime->http_code = 0;
    runtime->transport_error = 0;

    pthread_t worker;
    const int error = pthread_create(&worker, NULL, remote_worker, runtime);
    if (error) {
        runtime->busy = false;
        runtime->state = CLOUD360_CATALOG_WORKER_FAILED;
        runtime->transport_error = error;
        (void)pthread_mutex_unlock(&runtime->mutex);
        return false;
    }
    (void)pthread_detach(worker);
    (void)pthread_mutex_unlock(&runtime->mutex);
    return true;
}

bool cloud360_catalog_runtime_source(Cloud360CatalogRuntime *runtime,
                                     const char *source_file,
                                     const char *ca_file)
{
    if (!runtime || !runtime->initialized || !source_file || !ca_file)
        return false;
    Cloud360HttpsRequest request = { 0 };
    const size_t ca_len = strlen(ca_file);
    if (ca_len == 0 || ca_len >= sizeof request.ca_file)
        return false;
    memcpy(request.ca_file, ca_file, ca_len + 1);

    FILE *file = fopen(source_file, "rb");
    int fields = 0;
    if (file) {
        char extra[2] = { 0 };
        fields = fscanf(file, "%63s %95s %1s",
                        request.host, request.path, extra);
        if (fclose(file) != 0) fields = 0;
    }
    if (fields != 2 || !cloud360_https_request_valid(&request)) {
        (void)pthread_mutex_lock(&runtime->mutex);
        if (!runtime->busy) runtime->state = CLOUD360_CATALOG_NO_SOURCE;
        (void)pthread_mutex_unlock(&runtime->mutex);
        return false;
    }
    return cloud360_catalog_runtime_start(runtime, &request);
}

void cloud360_catalog_runtime_snapshot(Cloud360CatalogRuntime *runtime,
                                       uint32_t selected, Cloud360CatalogView *view)
{
    if (!view) return;
    memset(view, 0, sizeof *view);
    if (!runtime || !runtime->initialized) return;

    (void)pthread_mutex_lock(&runtime->mutex);
    view->state = runtime->state;
    view->https_status = runtime->https_status;
    view->transport_error = runtime->transport_error;
    view->http_code = runtime->http_code;
    view->busy = runtime->busy;
    view->loaded = runtime->active.loaded;
    view->count = runtime->active.count;

    if (view->count) {
        if (selected >= view->count) selected = 0;
        view->selected_index = selected;
        view->first_index = (selected / CLOUD360_CATALOG_VISIBLE) *
                            CLOUD360_CATALOG_VISIBLE;
        for (uint32_t i = view->first_index;
             i < view->count &&
             view->visible_count < CLOUD360_CATALOG_VISIBLE; ++i)
            view->visible[view->visible_count++] = runtime->active.games[i];
    }
    (void)pthread_mutex_unlock(&runtime->mutex);
}

const char *cloud360_catalog_state_text(Cloud360CatalogState status)
{
    switch (status) {
        case CLOUD360_CATALOG_EMPTY: return "CATALOGO NAO CARREGADO";
        case CLOUD360_CATALOG_LOCAL_READY: return "CATALOGO LOCAL CARREGADO";
        case CLOUD360_CATALOG_FILE_MISSING: return "catalog.json AUSENTE";
        case CLOUD360_CATALOG_INVALID_JSON: return "CATALOGO JSON INVALIDO";
        case CLOUD360_CATALOG_NO_SOURCE: return "FONTE HTTPS NAO CONFIGURADA";
        case CLOUD360_CATALOG_LOADING: return "BAIXANDO CATALOGO...";
        case CLOUD360_CATALOG_ONLINE_READY: return "CATALOGO HTTPS CARREGADO";
        case CLOUD360_CATALOG_HTTPS_FAILED: return "FALHA DE DOWNLOAD HTTPS";
        case CLOUD360_CATALOG_WORKER_FAILED: return "FALHA AO INICIAR DOWNLOAD";
        default: return "ESTADO DESCONHECIDO";
    }
}
