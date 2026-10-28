#include "probe.h"

#include <errno.h>
#include <string.h>

static void set_status(Cloud360Probe *probe, Cloud360ProbeStatus status,
                       uint32_t address, int error, bool busy)
{
    (void)pthread_mutex_lock(&probe->mutex);
    probe->result.status = status;
    probe->result.address_be = address;
    probe->result.error = error;
    probe->result.busy = busy;
    (void)pthread_mutex_unlock(&probe->mutex);
}

static void *run_probe(void *context)
{
    Cloud360Probe *probe = context;
    uint32_t address = 0;
    const int dns = probe->ops.resolve_ipv4(probe->ops.context, probe->host, &address);

    if (dns != 0 || address == 0) {
        set_status(probe, CLOUD360_PROBE_DNS_ERROR, 0,
                   dns ? dns : EADDRNOTAVAIL, false);
        return NULL;
    }

    set_status(probe, CLOUD360_PROBE_TCP, address, 0, true);
    const int tcp = probe->ops.connect_tcp(probe->ops.context,
                                           address, probe->port,
                                           CLOUD360_PROBE_TCP_TIMEOUT_MS);
    set_status(probe, tcp ? CLOUD360_PROBE_TCP_ERROR : CLOUD360_PROBE_CONNECTED,
               address, tcp, false);
    return NULL;
}

int cloud360_probe_init(Cloud360Probe *probe)
{
    if (!probe)
        return EINVAL;

    *probe = (Cloud360Probe){ 0 };
    const int error = pthread_mutex_init(&probe->mutex, NULL);
    if (error)
        return error;
    probe->initialized = true;
    probe->result.status = CLOUD360_PROBE_IDLE;
    return 0;
}

bool cloud360_probe_start(Cloud360Probe *probe, const Cloud360ProbeOps *ops,
                          const char *host, uint16_t port)
{
    if (!probe || !probe->initialized || !ops ||
        !ops->resolve_ipv4 || !ops->connect_tcp || !host || !port)
        return false;

    const size_t length = strlen(host);
    if (!length || length >= sizeof probe->host)
        return false;

    (void)pthread_mutex_lock(&probe->mutex);
    if (probe->result.busy) {
        (void)pthread_mutex_unlock(&probe->mutex);
        return false;
    }

    probe->ops = *ops;
    memcpy(probe->host, host, length + 1);
    probe->port = port;
    probe->result.status = CLOUD360_PROBE_DNS;
    probe->result.error = 0;
    probe->result.address_be = 0;
    probe->result.busy = true;
    ++probe->result.attempts;

    pthread_t thread;
    const int error = pthread_create(&thread, NULL, run_probe, probe);
    if (error) {
        probe->result.status = CLOUD360_PROBE_THREAD_ERROR;
        probe->result.error = error;
        probe->result.busy = false;
        (void)pthread_mutex_unlock(&probe->mutex);
        return false;
    }

    (void)pthread_detach(thread);
    (void)pthread_mutex_unlock(&probe->mutex);
    return true;
}

void cloud360_probe_snapshot(Cloud360Probe *probe, Cloud360ProbeSnapshot *out)
{
    if (!out)
        return;
    *out = (Cloud360ProbeSnapshot){ 0 };
    if (!probe || !probe->initialized)
        return;
    (void)pthread_mutex_lock(&probe->mutex);
    *out = probe->result;
    (void)pthread_mutex_unlock(&probe->mutex);
}

const char *cloud360_probe_status_text(Cloud360ProbeStatus status)
{
    switch (status) {
        case CLOUD360_PROBE_IDLE: return "NAO EXECUTADO";
        case CLOUD360_PROBE_DNS: return "CONSULTANDO DNS...";
        case CLOUD360_PROBE_TCP: return "TESTANDO TCP 443...";
        case CLOUD360_PROBE_CONNECTED: return "TCP 443 ACESSIVEL";
        case CLOUD360_PROBE_DNS_ERROR: return "FALHA NA CONSULTA DNS";
        case CLOUD360_PROBE_TCP_ERROR: return "FALHA DE CONEXAO TCP";
        case CLOUD360_PROBE_THREAD_ERROR: return "FALHA AO INICIAR TESTE";
        default: return "ESTADO DESCONHECIDO";
    }
}
