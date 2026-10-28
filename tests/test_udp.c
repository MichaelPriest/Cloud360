#define _POSIX_C_SOURCE 200809L
#include "../src/udp.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint8_t loopback[4] = { 127,0,0,1 };

int main(void)
{
    Cloud360UdpSocket a = { .fd = -1 }, b = { .fd = -1 };
    uint16_t a_port = 0, b_port = 0;
    assert(cloud360_udp_open(&a,loopback,0)==0);
    assert(cloud360_udp_open(&b,loopback,0)==0);
    assert(cloud360_udp_local_port(&a,&a_port)==0 && a_port);
    assert(cloud360_udp_local_port(&b,&b_port)==0 && b_port);
    assert(cloud360_udp_open(&a,loopback,0)==EINVAL);
    assert(cloud360_udp_receive(&b,0,NULL,NULL,0,NULL)==EINVAL);

    uint8_t incoming[CLOUD360_UDP_DATAGRAM_MAX];
    size_t bytes=99;
    Cloud360UdpPeer from = { 0 };
    assert(cloud360_udp_receive(&b,0,&from,incoming,sizeof incoming,
                                &bytes)==ETIMEDOUT);
    assert(bytes==0);
    assert(cloud360_udp_receive(&b,5001,&from,incoming,sizeof incoming,
                                &bytes)==EINVAL);
    const Cloud360UdpPeer target = {{ 127,0,0,1 }, b_port};
    const uint8_t message[] = { 0,1,2,3,4,5,6,7,8 };
    assert(cloud360_udp_send(&a,&target,message,sizeof message)==0);
    assert(cloud360_udp_receive(&b,1000,&from,incoming,sizeof incoming,
                                &bytes)==0);
    assert(bytes==sizeof message);
    assert(!memcmp(incoming,message,sizeof message));
    assert(from.port==a_port);
    assert(!memcmp(from.ip,loopback,4));

    /* Receive must discard oversized datagrams without copying partials.
     * A subsequent valid message remains receivable. */
    uint8_t large[CLOUD360_UDP_DATAGRAM_MAX + 1];
    memset(large,0x55,sizeof large);
    assert(cloud360_udp_send(&a,&target,large,sizeof large)==EINVAL);
    uint8_t small[4]={0};
    assert(cloud360_udp_send(&a,&target,message,sizeof message)==0);
    assert(cloud360_udp_receive(&b,1000,&from,small,sizeof small,&bytes)
           ==EMSGSIZE);
    assert(bytes==0 && small[0]==0);

    const Cloud360UdpPeer invalid = {{0,0,0,0},b_port};
    assert(cloud360_udp_send(&a,&invalid,message,sizeof message)
           ==EADDRNOTAVAIL);
    assert(cloud360_udp_send(&a,&target,message,0)==EINVAL);

    cloud360_udp_close(&a);
    cloud360_udp_close(&b);
    assert(a.fd==-1 && b.fd==-1);
    cloud360_udp_close(&a); /* idempotent */
    assert(cloud360_udp_local_port(&a,&a_port)==EINVAL);
    assert(cloud360_udp_send(&a,&target,message,sizeof message)==EINVAL);
    puts("Cloud360 bounded nonblocking IPv4/UDP loopback and errors: PASS");
    return 0;
}
