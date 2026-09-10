// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_ETH_H
#define DAS_ETH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <das/result.h>

/** Opaque Ethernet MAC resource handle. storage is backend-owned. */
typedef struct das_eth {
    uint32_t storage;
} das_eth_t;

#define DAS_ETH_INVALID ((das_eth_t){UINT32_MAX})
#define DAS_ETH_MAC_ADDRESS_SIZE 6u

/** Layer-2 Ethernet MAC configuration. */
typedef struct das_eth_config {
    uint8_t mac[DAS_ETH_MAC_ADDRESS_SIZE];
} das_eth_config_t;

typedef enum das_eth_duplex {
    DAS_ETH_DUPLEX_UNKNOWN = 0,
    DAS_ETH_DUPLEX_HALF,
    DAS_ETH_DUPLEX_FULL
} das_eth_duplex_t;

/** Current physical link state reported by the MAC/PHY backend. */
typedef struct das_eth_link_state {
    bool up;
    uint32_t speed_mbps;
    das_eth_duplex_t duplex;
} das_eth_link_state_t;

/** Return true when the selected backend recognizes this Ethernet handle. */
bool das_eth_is_valid(das_eth_t eth);

/** Configure the Ethernet MAC/PHY route selected by the board/device layer. */
das_result_t das_eth_init(das_eth_t eth, const das_eth_config_t* config);

/**
 * Send one complete Layer-2 Ethernet frame.
 *
 * The caller supplies the Ethernet header and payload. The backend owns any
 * device-specific DMA descriptor and cache-coherency work required to place
 * the frame on the wire.
 */
das_result_t das_eth_send(das_eth_t eth, const uint8_t* frame, size_t length);

/**
 * Poll for one complete Layer-2 Ethernet frame.
 *
 * On success, *received is the number of bytes copied to buffer. When no frame
 * is currently available this function returns DAS_OK with *received == 0.
 * The backend owns any device-specific RX DMA and cache-coherency handling.
 */
das_result_t das_eth_receive(das_eth_t eth,
                             uint8_t* buffer,
                             size_t capacity,
                             size_t* received);

/** Query the current PHY link state, negotiated speed and duplex mode. */
das_result_t das_eth_link_state(das_eth_t eth, das_eth_link_state_t* state);

#endif
