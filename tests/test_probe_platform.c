#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../src/probe.h"

int main(void)
{
    uint32_t ipv4 = 0;
    assert(cloud360_probe_resolve_ipv4(NULL, NULL, &ipv4) == EINVAL);
    assert(cloud360_probe_resolve_ipv4(NULL, "127.0.0.1", &ipv4) == 0);
    assert(ipv4 != 0);
    assert(cloud360_probe_connect_tcp(NULL, ipv4, 0, 500) == EINVAL);
    assert(cloud360_probe_connect_tcp(NULL, ipv4, 443, 0) == EINVAL);

    const int listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener >= 0);
    const struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port = 0,
        .sin_addr = { .s_addr = ipv4 }
    };
    assert(bind(listener, (const struct sockaddr *)&local, sizeof local) == 0);
    assert(listen(listener, 2) == 0);
    struct sockaddr_in assigned = { 0 };
    socklen_t size = sizeof assigned;
    assert(getsockname(listener, (struct sockaddr *)&assigned, &size) == 0);

    assert(cloud360_probe_connect_tcp(NULL, ipv4, ntohs(assigned.sin_port),
                                       500) == 0);
    assert(close(listener) == 0);

    /* The known-closed port should fail without a 2.5s blocking hang. */
    assert(cloud360_probe_connect_tcp(NULL, ipv4, ntohs(assigned.sin_port),
                                       500) != 0);
    puts("Cloud360 POSIX DNS and non-blocking TCP: PASS");
    return 0;
}
