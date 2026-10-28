#ifndef CLOUD360_PROBE_H
#define CLOUD360_PROBE_H

#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>

/* This is a DNS + TCP reachability diagnostic, NOT HTTPS, Xbox sign-in,
 * xCloud API access, or a streaming connection. */
enum {
    CLOUD360_PROBE_TCP_TIMEOUT_MS = 2500,
    CLOUD360_PROBE_HOST_LENGTH = 64
};

typedef enum {
    CLOUD360_PROBE_IDLE = 0,
    CLOUD360_PROBE_DNS,
    CLOUD360_PROBE_TCP,
    CLOUD360_PROBE_CONNECTED,
    CLOUD360_PROBE_DNS_ERROR,
    CLOUD360_PROBE_TCP_ERROR,
    CLOUD360_PROBE_THREAD_ERROR
} Cloud360ProbeStatus;

typedef struct {
    int (*resolve_ipv4)(void *context, const char *host, uint32_t *ip_be);
    int (*connect_tcp)(void *context, uint32_t ip_be,
                       uint16_t port, uint32_t timeout_ms);
    void *context;
} Cloud360ProbeOps;

typedef struct {
    Cloud360ProbeStatus status;
    uint32_t address_be;
    int error;
    uint32_t attempts;
    bool busy;
} Cloud360ProbeSnapshot;

/* The instance must live until the title exits. The background worker is
 * detached, and may be inside XAM's blocking DNS resolver when the user exits. */
typedef struct {
    pthread_mutex_t mutex;
    bool initialized;
    Cloud360ProbeSnapshot result;
    Cloud360ProbeOps ops;
    char host[CLOUD360_PROBE_HOST_LENGTH];
    uint16_t port;
} Cloud360Probe;

int cloud360_probe_init(Cloud360Probe *probe);
bool cloud360_probe_start(Cloud360Probe *probe, const Cloud360ProbeOps *ops,
                          const char *host, uint16_t port);
void cloud360_probe_snapshot(Cloud360Probe *probe, Cloud360ProbeSnapshot *out);
const char *cloud360_probe_status_text(Cloud360ProbeStatus status);

/* Console adapter, based on XSDK's POSIX IPv4 DNS and socket functions.
 * Returns positive errno-like values, 0 on success. */
int cloud360_probe_resolve_ipv4(void *context, const char *host, uint32_t *ip_be);
int cloud360_probe_connect_tcp(void *context, uint32_t ip_be,
                               uint16_t port, uint32_t timeout_ms);

#endif
