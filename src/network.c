#include "network.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

enum { LINK_ACTIVE = 0x01 };

void cloud360_network_init(Cloud360Network *network)
{
    *network = (Cloud360Network){ 0 };
    network->status = CLOUD360_NET_UNCHECKED;
    network->ipv4[0] = '\0';
}

const char *cloud360_network_status_text(Cloud360NetworkStatus status)
{
    switch (status) {
        case CLOUD360_NET_UNCHECKED: return "NAO VERIFICADA";
        case CLOUD360_NET_NO_LINK: return "SEM LINK DE REDE";
        case CLOUD360_NET_WAIT_IP: return "AGUARDANDO IP";
        case CLOUD360_NET_LOCAL_IP: return "IP LOCAL DISPONIVEL";
        case CLOUD360_NET_ERROR: return "ERRO DE REDE";
        default: return "ESTADO DESCONHECIDO";
    }
}

bool cloud360_network_poll(Cloud360Network *network, const Cloud360NetworkOps *ops,
                           uint32_t now_ms, bool force)
{
    if (!network || !ops || !ops->link || !ops->address)
        return false;

    /* Signed subtraction handles the 32-bit millisecond counter wrapping. */
    if (!force && network->has_checked &&
        (int32_t)(now_ms - network->next_checked_ms) < 0)
        return false;

    const Cloud360Network old = *network;
    network->last_checked_ms = now_ms;
    network->has_checked = true;
    network->checks++;
    network->error = 0;
    network->link_flags = 0;
    network->ipv4[0] = '\0';

    uint32_t link = 0;
    const int link_error = ops->link(ops->context, &link);
    if (link_error != 0) {
        network->status = CLOUD360_NET_ERROR;
        network->error = link_error;
    } else {
        network->link_flags = link;
        if ((link & LINK_ACTIVE) == 0) {
            network->status = CLOUD360_NET_NO_LINK;
        } else {
            uint32_t ipv4 = 0;
            const int address_error = ops->address(ops->context, &ipv4);
            if (address_error == EAGAIN || address_error == EADDRNOTAVAIL ||
                (address_error == 0 && ipv4 == 0)) {
                network->status = CLOUD360_NET_WAIT_IP;
            } else if (address_error != 0) {
                network->status = CLOUD360_NET_ERROR;
                network->error = address_error;
            } else {
                /* in_addr.s_addr is in network byte order, including on PPC. */
                const unsigned char *b = (const unsigned char *)&ipv4;
                (void)snprintf(network->ipv4, sizeof network->ipv4, "%u.%u.%u.%u",
                               (unsigned)b[0], (unsigned)b[1],
                               (unsigned)b[2], (unsigned)b[3]);
                network->status = CLOUD360_NET_LOCAL_IP;
            }
        }
    }

    network->next_checked_ms = now_ms +
        (network->status == CLOUD360_NET_LOCAL_IP ?
         CLOUD360_NETWORK_REFRESH_MS : CLOUD360_NETWORK_RETRY_MS);

    return old.status != network->status ||
           old.link_flags != network->link_flags ||
           old.error != network->error ||
           strcmp(old.ipv4, network->ipv4) != 0;
}
