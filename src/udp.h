#ifndef CLOUD360_UDP_H
#define CLOUD360_UDP_H

#include <stddef.h>
#include <stdint.h>

/* Simple, bounded IPv4/UDP socket for XSDK BSD sockets.
 * This is NOT an ICE agent. Opening sockets / sending datagrams
 * is never automatic in production and requires a future trusted
 * transport adapter. No Microsoft server is hardcoded. */
enum { CLOUD360_UDP_DATAGRAM_MAX = 1500, CLOUD360_UDP_WAIT_MAX_MS = 5000 };

typedef struct { uint8_t ip[4]; uint16_t port; } Cloud360UdpPeer;
typedef struct { int fd; } Cloud360UdpSocket;

/* Socket must be initialized to fd=-1 before first open.
 * bind_ip may be NULL to bind all local IPv4 interfaces. Port 0
 * requests an ephemeral port. Functions return 0 or an errno code. */
int cloud360_udp_open(Cloud360UdpSocket *udp, const uint8_t bind_ip[4],
                      uint16_t local_port);
int cloud360_udp_local_port(const Cloud360UdpSocket *udp, uint16_t *port);
int cloud360_udp_send(const Cloud360UdpSocket *udp,
                      const Cloud360UdpPeer *peer,
                      const uint8_t *data, size_t bytes);
int cloud360_udp_receive(const Cloud360UdpSocket *udp,
                         uint32_t timeout_ms,
                         Cloud360UdpPeer *from,
                         uint8_t *output, size_t capacity, size_t *received);
void cloud360_udp_close(Cloud360UdpSocket *udp);
#endif
