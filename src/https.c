#include "https.h"

#include <errno.h>
#include <string.h>

static size_t limited_length(const char *str, size_t max)
{
    size_t n = 0;
    while (n < max && str[n])
        ++n;
    return n;
}

bool cloud360_https_request_valid(const Cloud360HttpsRequest *request)
{
    if (!request)
        return false;

    const size_t h = limited_length(request->host, sizeof request->host);
    const size_t p = limited_length(request->path, sizeof request->path);
    const size_t ca = limited_length(request->ca_file, sizeof request->ca_file);
    if (h == 0 || h >= sizeof request->host ||
        p == 0 || p >= sizeof request->path ||
        ca == 0 || ca >= sizeof request->ca_file ||
        request->path[0] != '/' ||
        request->host[0] == '-' || request->host[h - 1] == '-')
        return false;

    for (size_t i = 0; i < h; ++i) {
        const char c = request->host[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-'))
            return false;
    }
    /* The path is URL-encoded, no controls, scheme, backslashes or fragments.
     * Cookies, credentials and authentication are not part of this client. */
    for (size_t i = 0; i < p; ++i) {
        const char c = request->path[i];
        const bool allowed =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '/' || c == '.' || c == '-' ||
            c == '_' || c == '~' || c == '?' || c == '&' || c == '=' ||
            c == '+' || c == '%';
        if (!allowed)
            return false;
    }
    if (p > 1 && request->path[1] == '/')
        return false;
    for (size_t i = 0; i < ca; ++i) {
        const char c = request->ca_file[i];
        if ((unsigned char)c < 33 || (unsigned char)c > 126)
            return false;
    }
    return true;
}

static void *worker(void *opaque)
{
    Cloud360Https *client = opaque;
    long http_status = 0;
    size_t bytes = 0;
    int error = 0;
    Cloud360HttpsStatus status = client->transport.fetch(
        client->transport.context, &client->request, &http_status, &bytes, &error);

    /* Fail closed if a transport incorrectly reports success. */
    if (status == CLOUD360_HTTPS_OK) {
        if (http_status < 200 || http_status >= 300)
            status = CLOUD360_HTTPS_HTTP_ERROR;
        if (bytes > CLOUD360_HTTPS_BODY_LIMIT)
            status = CLOUD360_HTTPS_TOO_LARGE;
    }

    (void)pthread_mutex_lock(&client->mutex);
    client->result.status = status;
    client->result.http_status = http_status;
    client->result.bytes_received = bytes;
    client->result.transport_error = error;
    client->result.busy = false;
    (void)pthread_mutex_unlock(&client->mutex);
    return NULL;
}

int cloud360_https_init(Cloud360Https *client)
{
    if (!client)
        return EINVAL;
    *client = (Cloud360Https){ 0 };
    const int err = pthread_mutex_init(&client->mutex, NULL);
    if (err)
        return err;
    client->initialized = true;
    client->result.status = CLOUD360_HTTPS_IDLE;
    return 0;
}

bool cloud360_https_start(Cloud360Https *client,
                          const Cloud360HttpsTransport *transport,
                          const Cloud360HttpsRequest *request)
{
    if (!client || !client->initialized || !transport || !transport->fetch ||
        !cloud360_https_request_valid(request))
        return false;

    (void)pthread_mutex_lock(&client->mutex);
    if (client->result.busy) {
        (void)pthread_mutex_unlock(&client->mutex);
        return false;
    }

    client->request = *request;
    client->transport = *transport;
    client->result.status = CLOUD360_HTTPS_RUNNING;
    client->result.http_status = 0;
    client->result.bytes_received = 0;
    client->result.transport_error = 0;
    client->result.busy = true;
    ++client->result.attempts;

    pthread_t thread;
    const int err = pthread_create(&thread, NULL, worker, client);
    if (err != 0) {
        client->result.status = CLOUD360_HTTPS_UNAVAILABLE;
        client->result.transport_error = err;
        client->result.busy = false;
        (void)pthread_mutex_unlock(&client->mutex);
        return false;
    }
    (void)pthread_detach(thread);
    (void)pthread_mutex_unlock(&client->mutex);
    return true;
}

void cloud360_https_snapshot(Cloud360Https *client, Cloud360HttpsSnapshot *result)
{
    if (!result)
        return;
    *result = (Cloud360HttpsSnapshot){ 0 };
    if (!client || !client->initialized)
        return;
    (void)pthread_mutex_lock(&client->mutex);
    *result = client->result;
    (void)pthread_mutex_unlock(&client->mutex);
}

const char *cloud360_https_status_text(Cloud360HttpsStatus status)
{
    switch (status) {
        case CLOUD360_HTTPS_IDLE: return "HTTPS NAO TESTADO";
        case CLOUD360_HTTPS_RUNNING: return "TESTANDO HTTPS...";
        case CLOUD360_HTTPS_OK: return "HTTPS CERTIFICADO OK";
        case CLOUD360_HTTPS_BAD_REQUEST: return "REQUISICAO INVALIDA";
        case CLOUD360_HTTPS_UNAVAILABLE: return "TLS NAO INTEGRADO";
        case CLOUD360_HTTPS_CA_MISSING: return "CA CERTIFICADO AUSENTE";
        case CLOUD360_HTTPS_TLS_ERROR: return "FALHA TLS/CERTIFICADO";
        case CLOUD360_HTTPS_TIMEOUT: return "TEMPO ESGOTADO";
        case CLOUD360_HTTPS_TOO_LARGE: return "RESPOSTA MUITO GRANDE";
        case CLOUD360_HTTPS_HTTP_ERROR: return "ERRO HTTP DO SERVIDOR";
        case CLOUD360_HTTPS_TRANSPORT_ERROR: return "FALHA NA CONEXAO";
        default: return "ESTADO DESCONHECIDO";
    }
}
