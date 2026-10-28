#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../src/probe.h"

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool gate;
    bool entered;
    int dns_error;
    int tcp_error;
    int dns_calls;
    int tcp_calls;
    uint32_t ip;
} Mock;

static int mock_dns(void *context, const char *host, uint32_t *ip)
{
    Mock *mock = context;
    assert(strcmp(host, "www.xbox.com") == 0);
    (void)pthread_mutex_lock(&mock->lock);
    mock->entered = true;
    mock->dns_calls++;
    (void)pthread_cond_broadcast(&mock->cond);
    while (mock->gate)
        (void)pthread_cond_wait(&mock->cond, &mock->lock);
    const int error = mock->dns_error;
    *ip = mock->ip;
    (void)pthread_mutex_unlock(&mock->lock);
    return error;
}

static int mock_tcp(void *context, uint32_t ip, uint16_t port, uint32_t timeout)
{
    Mock *mock = context;
    assert(ip == mock->ip && port == 443);
    assert(timeout == CLOUD360_PROBE_TCP_TIMEOUT_MS);
    (void)pthread_mutex_lock(&mock->lock);
    mock->tcp_calls++;
    const int error = mock->tcp_error;
    (void)pthread_mutex_unlock(&mock->lock);
    return error;
}

static void wait_until(Cloud360Probe *probe, Cloud360ProbeStatus status)
{
    Cloud360ProbeSnapshot s;
    struct timespec delay = { .tv_sec = 0, .tv_nsec = 5000000 };
    for (int i = 0; i < 200; ++i) {
        cloud360_probe_snapshot(probe, &s);
        if (s.status == status)
            return;
        (void)nanosleep(&delay, NULL);
    }
    fprintf(stderr, "Timeout waiting for status %d\n", (int)status);
    assert(0);
}

int main(void)
{
    assert(cloud360_probe_init(NULL) == EINVAL);
    static Cloud360Probe probe;
    assert(cloud360_probe_init(&probe) == 0);

    Mock mock = { .gate = true, .ip = 0x01020304u };
    assert(pthread_mutex_init(&mock.lock, NULL) == 0);
    assert(pthread_cond_init(&mock.cond, NULL) == 0);
    Cloud360ProbeOps ops = { mock_dns, mock_tcp, &mock };
    Cloud360ProbeSnapshot result;

    cloud360_probe_snapshot(&probe, &result);
    assert(result.status == CLOUD360_PROBE_IDLE && result.attempts == 0);
    assert(!cloud360_probe_start(&probe, &ops, "", 443));
    assert(!cloud360_probe_start(&probe, &ops, "www.xbox.com", 0));

    assert(cloud360_probe_start(&probe, &ops, "www.xbox.com", 443));
    (void)pthread_mutex_lock(&mock.lock);
    while (!mock.entered)
        (void)pthread_cond_wait(&mock.cond, &mock.lock);
    (void)pthread_mutex_unlock(&mock.lock);
    cloud360_probe_snapshot(&probe, &result);
    assert(result.status == CLOUD360_PROBE_DNS && result.busy);
    assert(result.attempts == 1);
    assert(!cloud360_probe_start(&probe, &ops, "www.xbox.com", 443));

    (void)pthread_mutex_lock(&mock.lock);
    mock.gate = false;
    (void)pthread_cond_broadcast(&mock.cond);
    (void)pthread_mutex_unlock(&mock.lock);
    wait_until(&probe, CLOUD360_PROBE_CONNECTED);
    cloud360_probe_snapshot(&probe, &result);
    assert(!result.busy && result.address_be == mock.ip);
    assert(mock.tcp_calls == 1);

    mock.dns_error = EHOSTUNREACH;
    assert(cloud360_probe_start(&probe, &ops, "www.xbox.com", 443));
    wait_until(&probe, CLOUD360_PROBE_DNS_ERROR);
    cloud360_probe_snapshot(&probe, &result);
    assert(result.error == EHOSTUNREACH && !result.busy && result.attempts == 2);
    assert(mock.tcp_calls == 1);

    mock.dns_error = 0;
    mock.tcp_error = ECONNREFUSED;
    assert(cloud360_probe_start(&probe, &ops, "www.xbox.com", 443));
    wait_until(&probe, CLOUD360_PROBE_TCP_ERROR);
    cloud360_probe_snapshot(&probe, &result);
    assert(result.error == ECONNREFUSED && !result.busy && result.attempts == 3);
    assert(mock.tcp_calls == 2);
    assert(!strcmp(cloud360_probe_status_text(result.status),
                   "FALHA DE CONEXAO TCP"));

    assert(pthread_cond_destroy(&mock.cond) == 0);
    assert(pthread_mutex_destroy(&mock.lock) == 0);
    /* Detached worker's state is process-scoped, as on Xbox. */
    puts("Cloud360 async DNS/TCP probe: PASS");
    return 0;
}
