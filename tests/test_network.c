#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "../src/network.h"

typedef struct {
    uint32_t flags;
    uint32_t ip;
    int link_error;
    int addr_error;
    unsigned links;
    unsigned addresses;
} Mock;

static int link_status(void *context, uint32_t *flags)
{
    Mock *m = context;
    m->links++;
    *flags = m->flags;
    return m->link_error;
}

static int local_address(void *context, uint32_t *address)
{
    Mock *m = context;
    m->addresses++;
    *address = m->ip;
    return m->addr_error;
}

static void test_network_diagnostics(void)
{
    Cloud360Network state;
    cloud360_network_init(&state);
    assert(!state.has_checked && state.status == CLOUD360_NET_UNCHECKED);
    assert(strcmp(cloud360_network_status_text(state.status), "NAO VERIFICADA") == 0);

    Mock mock = { 0 };
    Cloud360NetworkOps ops = { link_status, local_address, &mock };

    assert(cloud360_network_poll(&state, &ops, 100, false));
    assert(state.status == CLOUD360_NET_NO_LINK);
    assert(state.link_flags == 0 && state.ipv4[0] == 0);
    assert(mock.links == 1 && mock.addresses == 0);
    assert(!cloud360_network_poll(&state, &ops, 1000, false));
    assert(mock.links == 1);

    mock.flags = 0x03;
    mock.addr_error = EAGAIN;
    assert(cloud360_network_poll(&state, &ops, 2600, false));
    assert(state.status == CLOUD360_NET_WAIT_IP);
    assert(mock.addresses == 1);
    assert(!cloud360_network_poll(&state, &ops, 2610, false));

    mock.addr_error = 0;
    assert(inet_pton(AF_INET, "192.168.0.42", &mock.ip) == 1);
    assert(cloud360_network_poll(&state, &ops, 2620, true));
    assert(state.status == CLOUD360_NET_LOCAL_IP);
    assert(strcmp(state.ipv4, "192.168.0.42") == 0);
    assert(state.next_checked_ms == 12620);
    assert(!cloud360_network_poll(&state, &ops, 12000, false));
    assert(!cloud360_network_poll(&state, &ops, 12620, false));
    assert(state.checks == 4);

    mock.addr_error = EIO;
    assert(cloud360_network_poll(&state, &ops, 13000, true));
    assert(state.status == CLOUD360_NET_ERROR && state.error == EIO);
    assert(state.ipv4[0] == '\0');

    mock.link_error = ENETDOWN;
    assert(!cloud360_network_poll(&state, &ops, 15000, false));
    assert(cloud360_network_poll(&state, &ops, 15500, false));
    assert(state.status == CLOUD360_NET_ERROR && state.error == ENETDOWN);
    assert(mock.addresses == 4);

    mock.link_error = 0;
    mock.addr_error = EADDRNOTAVAIL;
    assert(cloud360_network_poll(&state, &ops, 15600, true));
    assert(state.status == CLOUD360_NET_WAIT_IP);
}

static void test_rollover_and_bad_ops(void)
{
    Cloud360Network state;
    cloud360_network_init(&state);
    Mock mock = { 0 };
    Cloud360NetworkOps ops = { link_status, local_address, &mock };

    assert(cloud360_network_poll(&state, &ops, UINT32_MAX - 1000, false));
    assert(!cloud360_network_poll(&state, &ops, 200, false));
    assert(cloud360_network_poll(&state, &ops, 1800, false) == false);
    assert(mock.links == 2);

    assert(!cloud360_network_poll(&state, NULL, 3000, true));
    assert(!cloud360_network_poll(NULL, &ops, 3000, true));
}

int main(void)
{
    test_network_diagnostics();
    test_rollover_and_bad_ops();
    puts("Cloud360 local network diagnostics: PASS");
    return 0;
}
