#ifndef CLOUD360_NETWORK_H
#define CLOUD360_NETWORK_H

#include <stdbool.h>
#include <stdint.h>

/* Network diagnostics refer to local connectivity only, NOT Xbox Live/xCloud.
 * This module is independent of XSDK so it can be tested on a desktop. */
enum {
    CLOUD360_NETWORK_REFRESH_MS = 10000,
    CLOUD360_NETWORK_RETRY_MS = 2500,
    CLOUD360_IPV4_BYTES = 16
};

typedef enum {
    CLOUD360_NET_UNCHECKED = 0,
    CLOUD360_NET_NO_LINK,
    CLOUD360_NET_WAIT_IP,
    CLOUD360_NET_LOCAL_IP,
    CLOUD360_NET_ERROR
} Cloud360NetworkStatus;

typedef struct {
    int (*link)(void *context, uint32_t *flags);
    int (*address)(void *context, uint32_t *network_order_ipv4);
    void *context;
} Cloud360NetworkOps;

typedef struct {
    Cloud360NetworkStatus status;
    uint32_t link_flags;
    char ipv4[CLOUD360_IPV4_BYTES];
    int error;
    uint32_t last_checked_ms;
    uint32_t next_checked_ms;
    uint32_t checks;
    bool has_checked;
} Cloud360Network;

void cloud360_network_init(Cloud360Network *network);

/* Poll only when due (or forced). Returns true if state/IP/link changed.
 * On Xbox 360, use xsdk_net_link and xsdk_net_address for the callbacks. */
bool cloud360_network_poll(Cloud360Network *network, const Cloud360NetworkOps *ops,
                           uint32_t now_ms, bool force);

const char *cloud360_network_status_text(Cloud360NetworkStatus status);

#endif
