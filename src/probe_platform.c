/*
 * Xbox 360 XSDK adapter. DNS uses XAM's resolver; TCP uses non-blocking POSIX
 * sockets. POLLOUT is not enough: XAM does not support SO_ERROR, so verify
 * connection via getpeername and reject POLLERR/POLLHUP/POLLNVAL.
 */
#include "probe.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

int cloud360_probe_resolve_ipv4(void *context, const char *host, uint32_t *ip_be)
{
    (void)context;
    if (!host || !ip_be)
        return EINVAL;

    *ip_be = 0;
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };
    struct addrinfo *results = NULL;
    const int error = getaddrinfo(host, NULL, &hints, &results);
    if (error != 0)
        return EHOSTUNREACH; /* getaddrinfo uses EAI codes, not errno */

    for (const struct addrinfo *node = results; node; node = node->ai_next) {
        if (node->ai_family != AF_INET || !node->ai_addr ||
            node->ai_addrlen < sizeof(struct sockaddr_in))
            continue;
        const struct sockaddr_in *a = (const struct sockaddr_in *)node->ai_addr;
        *ip_be = a->sin_addr.s_addr;
        if (*ip_be)
            break;
    }
    freeaddrinfo(results);
    return *ip_be ? 0 : EADDRNOTAVAIL;
}

int cloud360_probe_connect_tcp(void *context, uint32_t ip_be,
                               uint16_t port, uint32_t timeout_ms)
{
    (void)context;
    if (!ip_be || !port || !timeout_ms || timeout_ms > 10000)
        return EINVAL;

    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        return errno ? errno : EIO;

    int failure = 0;
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        failure = errno ? errno : EIO;
    } else {
        const struct sockaddr_in target = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr = { .s_addr = ip_be }
        };
        if (connect(fd, (const struct sockaddr *)&target, sizeof target) != 0) {
            if (errno != EINPROGRESS && errno != EWOULDBLOCK) {
                failure = errno ? errno : EIO;
            } else {
                struct pollfd waiting = { .fd = fd, .events = POLLOUT };
                const int ready = poll(&waiting, 1, (int)timeout_ms);
                if (ready < 0)
                    failure = errno ? errno : EIO;
                else if (ready == 0)
                    failure = ETIMEDOUT;
                else if (waiting.revents & (POLLERR | POLLHUP | POLLNVAL))
                    failure = EIO;
                else if (!(waiting.revents & POLLOUT))
                    failure = EIO;
                else {
                    struct sockaddr_in peer = { 0 };
                    socklen_t length = sizeof peer;
                    if (getpeername(fd, (struct sockaddr *)&peer, &length) != 0)
                        failure = errno ? errno : EIO;
                }
            }
        }
    }

    (void)close(fd);
    return failure;
}
