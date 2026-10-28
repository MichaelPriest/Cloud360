#include "udp.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void addr_from_peer(struct sockaddr_in *addr,
                           const Cloud360UdpPeer *peer)
{
    memset(addr, 0, sizeof *addr);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(peer->port);
    memcpy(&addr->sin_addr.s_addr, peer->ip, 4);
}

int cloud360_udp_open(Cloud360UdpSocket *udp, const uint8_t bind_ip[4],
                      uint16_t local_port)
{
    if (!udp || udp->fd != -1)
        return EINVAL;
    const int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return errno ? errno : EIO;
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        const int error = errno ? errno : EIO;
        (void)close(fd);
        return error;
    }
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_port = htons(local_port);
    if (bind_ip)
        memcpy(&addr.sin_addr.s_addr, bind_ip, 4);
    else
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (const struct sockaddr *)&addr, sizeof addr)) {
        const int error = errno ? errno : EIO;
        (void)close(fd);
        return error;
    }
    udp->fd = fd;
    return 0;
}

int cloud360_udp_local_port(const Cloud360UdpSocket *udp, uint16_t *port)
{
    if (!udp || udp->fd < 0 || !port)
        return EINVAL;
    struct sockaddr_in addr = { 0 };
    socklen_t length = sizeof addr;
    if (getsockname(udp->fd, (struct sockaddr *)&addr, &length))
        return errno ? errno : EIO;
    if (length < sizeof addr || addr.sin_family != AF_INET)
        return EIO;
    *port = ntohs(addr.sin_port);
    return *port ? 0 : EADDRNOTAVAIL;
}

int cloud360_udp_send(const Cloud360UdpSocket *udp,
                      const Cloud360UdpPeer *peer,
                      const uint8_t *data, size_t bytes)
{
    if (!udp || udp->fd < 0 || !peer || !data || !bytes ||
        bytes > CLOUD360_UDP_DATAGRAM_MAX || !peer->port)
        return EINVAL;
    uint8_t nonzero = 0;
    for (size_t i = 0; i < 4; ++i) nonzero |= peer->ip[i];
    if (!nonzero) return EADDRNOTAVAIL;
    struct sockaddr_in addr;
    addr_from_peer(&addr, peer);
    const ssize_t n = sendto(udp->fd, data, bytes, 0,
                             (const struct sockaddr *)&addr, sizeof addr);
    if (n < 0) return errno ? errno : EIO;
    return (size_t)n == bytes ? 0 : EIO;
}

int cloud360_udp_receive(const Cloud360UdpSocket *udp, uint32_t timeout_ms,
                         Cloud360UdpPeer *from,
                         uint8_t *output, size_t capacity, size_t *received)
{
    if (received) *received = 0;
    if (!udp || udp->fd < 0 || !from || !output || !received ||
        !capacity || timeout_ms > CLOUD360_UDP_WAIT_MAX_MS)
        return EINVAL;
    struct pollfd wait = { .fd = udp->fd, .events = POLLIN };
    const int ready = poll(&wait, 1, (int)timeout_ms);
    if (ready == 0) return ETIMEDOUT;
    if (ready < 0) return errno ? errno : EIO;
    if ((wait.revents & (POLLERR | POLLNVAL | POLLHUP)) ||
        !(wait.revents & POLLIN))
        return EIO;
    /* One extra byte detects oversized datagrams, including when
     * the OS truncates packets to the supplied receive buffer.
     * The input is only copied after size checks pass. */
    uint8_t scratch[CLOUD360_UDP_DATAGRAM_MAX + 1];
    struct sockaddr_in address = { 0 };
    socklen_t addrlen = sizeof address;
    const ssize_t n = recvfrom(udp->fd, scratch, sizeof scratch, 0,
                                (struct sockaddr *)&address, &addrlen);
    if (n < 0) return errno ? errno : EIO;
    if (!n || (size_t)n > CLOUD360_UDP_DATAGRAM_MAX ||
        (size_t)n > capacity) return EMSGSIZE;
    if (addrlen < sizeof address || address.sin_family != AF_INET ||
        !address.sin_port) return EIO;
    Cloud360UdpPeer peer = { 0 };
    memcpy(peer.ip, &address.sin_addr.s_addr, 4);
    peer.port = ntohs(address.sin_port);
    memcpy(output, scratch, (size_t)n);
    *from = peer;
    *received = (size_t)n;
    return 0;
}

void cloud360_udp_close(Cloud360UdpSocket *udp)
{
    if (udp && udp->fd >= 0) {
        (void)close(udp->fd);
        udp->fd = -1;
    }
}
