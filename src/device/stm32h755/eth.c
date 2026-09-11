// SPDX-License-Identifier: Apache-2.0

#include <das/cache.h>
#include <das/eth.h>

#include "eth_internal.h"
#include "stm32h755xx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STM32H755_ETH_HANDLE_BASE UINT32_C(0x45544800)
#define STM32H755_ETH_HANDLE_MASK UINT32_C(0xffffff00)
#define STM32H755_ETH_INSTANCE_MASK UINT32_C(0x000000ff)

#define STM32H755_ETH_MIN_FRAME_SIZE 14u
#define STM32H755_ETH_TX_DESC_COUNT 4u
#define STM32H755_ETH_RX_DESC_COUNT 4u
#define STM32H755_ETH_BUFFER_SIZE 1536u
#define STM32H755_ETH_WAIT_LIMIT UINT32_C(1000000)
#define STM32H755_ETH_PHY_ADDRESS_COUNT 32u

/* LAN8742 Clause-22 registers and fields used by the board baseline. */
#define LAN8742_BCR UINT8_C(0x00)
#define LAN8742_BSR UINT8_C(0x01)
#define LAN8742_PHYI1R UINT8_C(0x02)
#define LAN8742_PHYI2R UINT8_C(0x03)
#define LAN8742_SMR UINT8_C(0x12)
#define LAN8742_PHYSCSR UINT8_C(0x1f)
#define LAN8742_BCR_AUTONEGO_EN UINT16_C(0x1000)
#define LAN8742_BCR_POWER_DOWN UINT16_C(0x0800)
#define LAN8742_BCR_ISOLATE UINT16_C(0x0400)
#define LAN8742_BCR_RESTART_AUTONEGO UINT16_C(0x0200)
#define LAN8742_BSR_LINK_STATUS UINT16_C(0x0004)
#define LAN8742_SMR_PHY_ADDR UINT16_C(0x001f)
#define LAN8742_PHYSCSR_HCDSPEEDMASK UINT16_C(0x001c)
#define LAN8742_PHYSCSR_10BT_HD UINT16_C(0x0004)
#define LAN8742_PHYSCSR_10BT_FD UINT16_C(0x0014)
#define LAN8742_PHYSCSR_100BTX_HD UINT16_C(0x0008)
#define LAN8742_PHYSCSR_100BTX_FD UINT16_C(0x0018)

/* STM32H7 normal descriptor bits. These fields are not exposed by CMSIS. */
#define STM32H755_ETH_TX_OWN UINT32_C(0x80000000)
#define STM32H755_ETH_TX_FD UINT32_C(0x20000000)
#define STM32H755_ETH_TX_LD UINT32_C(0x10000000)
#define STM32H755_ETH_TX_B1L UINT32_C(0x00003fff)
#define STM32H755_ETH_TX_FL UINT32_C(0x00007fff)
#define STM32H755_ETH_RX_OWN UINT32_C(0x80000000)
#define STM32H755_ETH_RX_FD UINT32_C(0x20000000)
#define STM32H755_ETH_RX_LD UINT32_C(0x10000000)
#define STM32H755_ETH_RX_BUF1V UINT32_C(0x01000000)
#define STM32H755_ETH_RX_ES UINT32_C(0x00008000)
#define STM32H755_ETH_RX_PL UINT32_C(0x00007fff)

typedef struct stm32h755_eth_descriptor {
    volatile uint32_t desc0;
    volatile uint32_t desc1;
    volatile uint32_t desc2;
    volatile uint32_t desc3;
    uint32_t padding[4];
} stm32h755_eth_descriptor_t;

_Static_assert(sizeof(stm32h755_eth_descriptor_t) == 32u,
               "STM32H755 Ethernet descriptor must occupy one CM7 cache line");

static bool mac_address_valid(const uint8_t mac[DAS_ETH_MAC_ADDRESS_SIZE]) {
    if ((mac[0] & UINT8_C(0x01)) != 0u) {
        return false;
    }

    uint8_t combined = 0u;
    for (size_t i = 0u; i < DAS_ETH_MAC_ADDRESS_SIZE; ++i) {
        combined |= mac[i];
    }
    return combined != 0u;
}

static stm32h755_eth_instance_t instance_from_handle(das_eth_t eth) {
    if ((eth.storage & STM32H755_ETH_HANDLE_MASK) != STM32H755_ETH_HANDLE_BASE) {
        return (stm32h755_eth_instance_t)0;
    }
    return (stm32h755_eth_instance_t)(eth.storage & STM32H755_ETH_INSTANCE_MASK);
}

das_eth_t stm32h755_eth_handle(stm32h755_eth_instance_t instance) {
    if (instance != STM32H755_ETH1) {
        return DAS_ETH_INVALID;
    }
    return (das_eth_t){STM32H755_ETH_HANDLE_BASE | (uint32_t)instance};
}

bool das_eth_is_valid(das_eth_t eth) {
    return instance_from_handle(eth) == STM32H755_ETH1;
}

bool stm32h755_eth_current_core_supported(void) {
#if defined(CORE_CM7)
    return true;
#else
    return false;
#endif
}

#if defined(CORE_CM7)

static stm32h755_eth_descriptor_t g_tx_desc[STM32H755_ETH_TX_DESC_COUNT]
    __attribute__((aligned(32)));
static stm32h755_eth_descriptor_t g_rx_desc[STM32H755_ETH_RX_DESC_COUNT]
    __attribute__((aligned(32)));
static uint8_t g_tx_buffers[STM32H755_ETH_TX_DESC_COUNT][STM32H755_ETH_BUFFER_SIZE]
    __attribute__((aligned(32)));
static uint8_t g_rx_buffers[STM32H755_ETH_RX_DESC_COUNT][STM32H755_ETH_BUFFER_SIZE]
    __attribute__((aligned(32)));

static bool g_initialized;
static uint8_t g_phy_address = UINT8_MAX;
static uint32_t g_tx_index;
static uint32_t g_rx_index;

static uint32_t address32(const void* address) {
    return (uint32_t)(uintptr_t)address;
}

static void byte_copy(uint8_t* destination, const uint8_t* source, size_t size) {
    for (size_t i = 0u; i < size; ++i) {
        destination[i] = source[i];
    }
}

static das_result_t wait_clear(volatile uint32_t* reg, uint32_t mask) {
    for (uint32_t poll = 0u; poll < STM32H755_ETH_WAIT_LIMIT; ++poll) {
        if ((*reg & mask) == 0u) {
            return DAS_OK;
        }
    }
    return DAS_ERROR_TIMEOUT;
}

static void enable_and_reset_mac(void) {
    RCC_C1->AHB1ENR |= RCC_AHB1ENR_ETH1MACEN |
                       RCC_AHB1ENR_ETH1TXEN |
                       RCC_AHB1ENR_ETH1RXEN;
    RCC_C1->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC_C1->AHB1ENR;
    (void)RCC_C1->APB4ENR;
    __DSB();

    RCC->AHB1RSTR |= RCC_AHB1RSTR_ETH1MACRST;
    __DSB();
    RCC->AHB1RSTR &= ~RCC_AHB1RSTR_ETH1MACRST;
    __DSB();

    SYSCFG->PMCR = (SYSCFG->PMCR & ~SYSCFG_PMCR_EPIS_SEL) |
                   SYSCFG_PMCR_EPIS_SEL_2;
    (void)SYSCFG->PMCR;
    __DSB();
}

static das_result_t mdio_wait_idle(void) {
    return wait_clear(&ETH->MACMDIOAR, ETH_MACMDIOAR_MB);
}

static void mdio_configure_clock(void) {
    ETH->MACMDIOAR = (ETH->MACMDIOAR & ~ETH_MACMDIOAR_CR) |
                     ETH_MACMDIOAR_CR_DIV124;
}

static das_result_t mdio_read(uint8_t phy_address,
                              uint8_t register_address,
                              uint16_t* value) {
    if (phy_address >= STM32H755_ETH_PHY_ADDRESS_COUNT ||
        register_address >= 32u || value == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    das_result_t result = mdio_wait_idle();
    if (result != DAS_OK) {
        return result;
    }

    uint32_t address = ETH->MACMDIOAR;
    address &= ~(ETH_MACMDIOAR_PA | ETH_MACMDIOAR_RDA |
                 ETH_MACMDIOAR_MOC | ETH_MACMDIOAR_C45E);
    address |= ((uint32_t)phy_address << ETH_MACMDIOAR_PA_Pos) & ETH_MACMDIOAR_PA;
    address |= ((uint32_t)register_address << ETH_MACMDIOAR_RDA_Pos) & ETH_MACMDIOAR_RDA;
    address |= ETH_MACMDIOAR_MOC_RD | ETH_MACMDIOAR_MB;
    ETH->MACMDIOAR = address;

    result = mdio_wait_idle();
    if (result != DAS_OK) {
        return result;
    }

    *value = (uint16_t)ETH->MACMDIODR;
    return DAS_OK;
}

static das_result_t mdio_write(uint8_t phy_address,
                               uint8_t register_address,
                               uint16_t value) {
    if (phy_address >= STM32H755_ETH_PHY_ADDRESS_COUNT || register_address >= 32u) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    das_result_t result = mdio_wait_idle();
    if (result != DAS_OK) {
        return result;
    }

    uint32_t address = ETH->MACMDIOAR;
    address &= ~(ETH_MACMDIOAR_PA | ETH_MACMDIOAR_RDA |
                 ETH_MACMDIOAR_MOC | ETH_MACMDIOAR_C45E);
    address |= ((uint32_t)phy_address << ETH_MACMDIOAR_PA_Pos) & ETH_MACMDIOAR_PA;
    address |= ((uint32_t)register_address << ETH_MACMDIOAR_RDA_Pos) & ETH_MACMDIOAR_RDA;
    address |= ETH_MACMDIOAR_MOC_WR | ETH_MACMDIOAR_MB;

    ETH->MACMDIODR = value;
    ETH->MACMDIOAR = address;
    return mdio_wait_idle();
}

static das_result_t discover_phy(void) {
    g_phy_address = UINT8_MAX;

    for (uint8_t address = 0u; address < STM32H755_ETH_PHY_ADDRESS_COUNT; ++address) {
        uint16_t smr = 0u;
        if (mdio_read(address, LAN8742_SMR, &smr) != DAS_OK ||
            (smr & LAN8742_SMR_PHY_ADDR) != address) {
            continue;
        }

        uint16_t id1 = 0u;
        uint16_t id2 = 0u;
        if (mdio_read(address, LAN8742_PHYI1R, &id1) != DAS_OK ||
            mdio_read(address, LAN8742_PHYI2R, &id2) != DAS_OK) {
            continue;
        }
        if ((id1 == 0u && id2 == 0u) ||
            (id1 == UINT16_MAX && id2 == UINT16_MAX)) {
            continue;
        }

        g_phy_address = address;
        return DAS_OK;
    }

    return DAS_ERROR_NOT_READY;
}

static das_result_t configure_phy(void) {
    uint16_t control = 0u;
    das_result_t result = mdio_read(g_phy_address, LAN8742_BCR, &control);
    if (result != DAS_OK) {
        return result;
    }

    control &= (uint16_t)~(LAN8742_BCR_POWER_DOWN | LAN8742_BCR_ISOLATE);
    control |= LAN8742_BCR_AUTONEGO_EN | LAN8742_BCR_RESTART_AUTONEGO;
    return mdio_write(g_phy_address, LAN8742_BCR, control);
}

static das_result_t cache_clean_descriptor(stm32h755_eth_descriptor_t* descriptor) {
    return das_cache_data_clean(descriptor, sizeof(*descriptor));
}

static das_result_t cache_invalidate_descriptor(stm32h755_eth_descriptor_t* descriptor) {
    return das_cache_data_invalidate(descriptor, sizeof(*descriptor));
}

static das_result_t initialize_descriptors(void) {
    g_tx_index = 0u;
    g_rx_index = 0u;

    for (uint32_t i = 0u; i < STM32H755_ETH_TX_DESC_COUNT; ++i) {
        g_tx_desc[i].desc0 = 0u;
        g_tx_desc[i].desc1 = 0u;
        g_tx_desc[i].desc2 = 0u;
        g_tx_desc[i].desc3 = 0u;
        for (size_t word = 0u; word < 4u; ++word) {
            g_tx_desc[i].padding[word] = 0u;
        }
        const das_result_t cache_result = cache_clean_descriptor(&g_tx_desc[i]);
        if (cache_result != DAS_OK) {
            return cache_result;
        }
    }

    for (uint32_t i = 0u; i < STM32H755_ETH_RX_DESC_COUNT; ++i) {
        das_result_t cache_result =
            das_cache_data_invalidate(g_rx_buffers[i], STM32H755_ETH_BUFFER_SIZE);
        if (cache_result != DAS_OK) {
            return cache_result;
        }

        g_rx_desc[i].desc0 = address32(g_rx_buffers[i]);
        g_rx_desc[i].desc1 = 0u;
        g_rx_desc[i].desc2 = 0u;
        g_rx_desc[i].desc3 = STM32H755_ETH_RX_OWN | STM32H755_ETH_RX_BUF1V;
        for (size_t word = 0u; word < 4u; ++word) {
            g_rx_desc[i].padding[word] = 0u;
        }
        cache_result = cache_clean_descriptor(&g_rx_desc[i]);
        if (cache_result != DAS_OK) {
            return cache_result;
        }
    }

    __DSB();

    ETH->DMACTDRLR = STM32H755_ETH_TX_DESC_COUNT - 1u;
    ETH->DMACTDLAR = address32(&g_tx_desc[0]);
    ETH->DMACTDTPR = address32(&g_tx_desc[0]);

    ETH->DMACRDRLR = STM32H755_ETH_RX_DESC_COUNT - 1u;
    ETH->DMACRDLAR = address32(&g_rx_desc[0]);
    ETH->DMACRDTPR = address32(&g_rx_desc[STM32H755_ETH_RX_DESC_COUNT - 1u]);
    return DAS_OK;
}

static void configure_mac_dma(const das_eth_config_t* config) {
    ETH->MACA0HR = ((uint32_t)config->mac[5] << 8u) |
                   (uint32_t)config->mac[4];
    ETH->MACA0LR = ((uint32_t)config->mac[3] << 24u) |
                   ((uint32_t)config->mac[2] << 16u) |
                   ((uint32_t)config->mac[1] << 8u) |
                   (uint32_t)config->mac[0];

    /* Start at the common 100 Mbit/s full-duplex mode; PHY status refreshes it. */
    ETH->MACCR = (ETH->MACCR & ~(ETH_MACCR_FES | ETH_MACCR_DM |
                                 ETH_MACCR_TE | ETH_MACCR_RE)) |
                 ETH_MACCR_FES | ETH_MACCR_DM | ETH_MACCR_ACS | ETH_MACCR_CST;

    ETH->MTLTQOMR |= ETH_MTLTQOMR_TSF;
    ETH->MTLRQOMR |= ETH_MTLRQOMR_RSF;
    ETH->DMASBMR |= ETH_DMASBMR_AAL | ETH_DMASBMR_FB;

    ETH->DMACCR = (ETH->DMACCR & ~ETH_DMACCR_DSL) | ETH_DMACCR_DSL_128BIT;
    ETH->DMACTCR = (ETH->DMACTCR & ~ETH_DMACTCR_TPBL) |
                   ETH_DMACTCR_TPBL_32PBL;
    ETH->DMACRCR = (ETH->DMACRCR & ~(ETH_DMACRCR_RPBL | ETH_DMACRCR_RBSZ)) |
                   ETH_DMACRCR_RPBL_32PBL |
                   (((uint32_t)STM32H755_ETH_BUFFER_SIZE << ETH_DMACRCR_RBSZ_Pos) &
                    ETH_DMACRCR_RBSZ);
    ETH->DMACIER = 0u;
}

static void start_mac_dma(void) {
    ETH->MACCR |= ETH_MACCR_TE | ETH_MACCR_RE;
    ETH->MTLTQOMR |= ETH_MTLTQOMR_FTQ;
    ETH->DMACTCR |= ETH_DMACTCR_ST;
    ETH->DMACRCR |= ETH_DMACRCR_SR;
    __DSB();
}

static void recycle_rx_descriptor(uint32_t index) {
    stm32h755_eth_descriptor_t* const descriptor = &g_rx_desc[index];

    (void)das_cache_data_invalidate(g_rx_buffers[index], STM32H755_ETH_BUFFER_SIZE);
    descriptor->desc0 = address32(g_rx_buffers[index]);
    descriptor->desc1 = 0u;
    descriptor->desc2 = 0u;
    descriptor->desc3 = STM32H755_ETH_RX_OWN | STM32H755_ETH_RX_BUF1V;
    __DMB();
    (void)cache_clean_descriptor(descriptor);
    __DSB();
    ETH->DMACRDTPR = address32(descriptor);
}

static das_result_t read_link_state(das_eth_link_state_t* state) {
    state->up = false;
    state->speed_mbps = 0u;
    state->duplex = DAS_ETH_DUPLEX_UNKNOWN;

    uint16_t status = 0u;
    das_result_t result = mdio_read(g_phy_address, LAN8742_BSR, &status);
    if (result != DAS_OK) {
        return result;
    }
    /* Link status is latch-low; the second read returns the current state. */
    result = mdio_read(g_phy_address, LAN8742_BSR, &status);
    if (result != DAS_OK) {
        return result;
    }
    if ((status & LAN8742_BSR_LINK_STATUS) == 0u) {
        return DAS_OK;
    }

    uint16_t phy_status = 0u;
    result = mdio_read(g_phy_address, LAN8742_PHYSCSR, &phy_status);
    if (result != DAS_OK) {
        return result;
    }

    switch (phy_status & LAN8742_PHYSCSR_HCDSPEEDMASK) {
        case LAN8742_PHYSCSR_100BTX_FD:
            state->speed_mbps = 100u;
            state->duplex = DAS_ETH_DUPLEX_FULL;
            break;
        case LAN8742_PHYSCSR_100BTX_HD:
            state->speed_mbps = 100u;
            state->duplex = DAS_ETH_DUPLEX_HALF;
            break;
        case LAN8742_PHYSCSR_10BT_FD:
            state->speed_mbps = 10u;
            state->duplex = DAS_ETH_DUPLEX_FULL;
            break;
        case LAN8742_PHYSCSR_10BT_HD:
            state->speed_mbps = 10u;
            state->duplex = DAS_ETH_DUPLEX_HALF;
            break;
        default:
            return DAS_ERROR_IO;
    }

    const uint32_t enable_bits = ETH->MACCR & (ETH_MACCR_TE | ETH_MACCR_RE);
    uint32_t maccr = ETH->MACCR & ~(ETH_MACCR_TE | ETH_MACCR_RE |
                                    ETH_MACCR_FES | ETH_MACCR_DM);
    if (state->speed_mbps == 100u) {
        maccr |= ETH_MACCR_FES;
    }
    if (state->duplex == DAS_ETH_DUPLEX_FULL) {
        maccr |= ETH_MACCR_DM;
    }
    ETH->MACCR = maccr | enable_bits;
    __DSB();

    state->up = true;
    return DAS_OK;
}

#endif /* CORE_CM7 */

das_result_t das_eth_init(das_eth_t eth, const das_eth_config_t* config) {
    if (!das_eth_is_valid(eth) || config == 0 || !mac_address_valid(config->mac)) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

#if !defined(CORE_CM7)
    return DAS_ERROR_UNSUPPORTED;
#else
    g_initialized = false;
    g_phy_address = UINT8_MAX;

    enable_and_reset_mac();

    ETH->DMAMR |= ETH_DMAMR_SWR;
    das_result_t result = wait_clear(&ETH->DMAMR, ETH_DMAMR_SWR);
    if (result != DAS_OK) {
        return result;
    }

    mdio_configure_clock();
    configure_mac_dma(config);

    result = initialize_descriptors();
    if (result != DAS_OK) {
        return result;
    }

    result = discover_phy();
    if (result != DAS_OK) {
        return result;
    }
    result = configure_phy();
    if (result != DAS_OK) {
        return result;
    }

    start_mac_dma();
    g_initialized = true;
    return DAS_OK;
#endif
}

das_result_t das_eth_send(das_eth_t eth, const uint8_t* frame, size_t length) {
    if (!das_eth_is_valid(eth) || frame == 0 ||
        length < STM32H755_ETH_MIN_FRAME_SIZE || length > DAS_ETH_MAX_FRAME_SIZE) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

#if !defined(CORE_CM7)
    return DAS_ERROR_UNSUPPORTED;
#else
    if (!g_initialized) {
        return DAS_ERROR_NOT_READY;
    }

    das_eth_link_state_t link;
    das_result_t result = read_link_state(&link);
    if (result != DAS_OK) {
        return result;
    }
    if (!link.up) {
        return DAS_ERROR_NOT_READY;
    }

    stm32h755_eth_descriptor_t* const descriptor = &g_tx_desc[g_tx_index];
    result = cache_invalidate_descriptor(descriptor);
    if (result != DAS_OK) {
        return result;
    }
    if ((descriptor->desc3 & STM32H755_ETH_TX_OWN) != 0u) {
        return DAS_ERROR_NOT_READY;
    }

    byte_copy(g_tx_buffers[g_tx_index], frame, length);
    result = das_cache_data_clean(g_tx_buffers[g_tx_index], length);
    if (result != DAS_OK) {
        return result;
    }

    descriptor->desc0 = address32(g_tx_buffers[g_tx_index]);
    descriptor->desc1 = 0u;
    descriptor->desc2 = (uint32_t)length & STM32H755_ETH_TX_B1L;
    descriptor->desc3 = ((uint32_t)length & STM32H755_ETH_TX_FL) |
                        STM32H755_ETH_TX_FD |
                        STM32H755_ETH_TX_LD;
    __DMB();
    descriptor->desc3 |= STM32H755_ETH_TX_OWN;
    result = cache_clean_descriptor(descriptor);
    if (result != DAS_OK) {
        return result;
    }
    __DSB();

    const uint32_t current = g_tx_index;
    const uint32_t next = (current + 1u) % STM32H755_ETH_TX_DESC_COUNT;
    g_tx_index = next;
    ETH->DMACTDTPR = address32(&g_tx_desc[next]);

    for (uint32_t poll = 0u; poll < STM32H755_ETH_WAIT_LIMIT; ++poll) {
        result = cache_invalidate_descriptor(&g_tx_desc[current]);
        if (result != DAS_OK) {
            return result;
        }
        if ((g_tx_desc[current].desc3 & STM32H755_ETH_TX_OWN) == 0u) {
            return DAS_OK;
        }
        if ((ETH->DMACSR & ETH_DMACSR_FBE) != 0u) {
            return DAS_ERROR_IO;
        }
    }

    return DAS_ERROR_TIMEOUT;
#endif
}

das_result_t das_eth_receive(das_eth_t eth,
                             uint8_t* buffer,
                             size_t capacity,
                             size_t* received) {
    if (!das_eth_is_valid(eth) || buffer == 0 || capacity == 0u || received == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }
    *received = 0u;

#if !defined(CORE_CM7)
    return DAS_ERROR_UNSUPPORTED;
#else
    if (!g_initialized) {
        return DAS_ERROR_NOT_READY;
    }

    const uint32_t index = g_rx_index;
    stm32h755_eth_descriptor_t* const descriptor = &g_rx_desc[index];
    das_result_t result = cache_invalidate_descriptor(descriptor);
    if (result != DAS_OK) {
        return result;
    }
    if ((descriptor->desc3 & STM32H755_ETH_RX_OWN) != 0u) {
        return DAS_OK;
    }

    const uint32_t status = descriptor->desc3;
    const size_t length = (size_t)(status & STM32H755_ETH_RX_PL);
    const bool complete =
        (status & (STM32H755_ETH_RX_FD | STM32H755_ETH_RX_LD)) ==
        (STM32H755_ETH_RX_FD | STM32H755_ETH_RX_LD);
    const bool valid = complete &&
                       (status & STM32H755_ETH_RX_ES) == 0u &&
                       length >= STM32H755_ETH_MIN_FRAME_SIZE &&
                       length <= DAS_ETH_MAX_FRAME_SIZE;

    if (!valid) {
        recycle_rx_descriptor(index);
        g_rx_index = (index + 1u) % STM32H755_ETH_RX_DESC_COUNT;
        return DAS_ERROR_IO;
    }
    if (length > capacity) {
        recycle_rx_descriptor(index);
        g_rx_index = (index + 1u) % STM32H755_ETH_RX_DESC_COUNT;
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    result = das_cache_data_invalidate(g_rx_buffers[index], length);
    if (result != DAS_OK) {
        return result;
    }
    byte_copy(buffer, g_rx_buffers[index], length);
    *received = length;

    recycle_rx_descriptor(index);
    g_rx_index = (index + 1u) % STM32H755_ETH_RX_DESC_COUNT;
    return DAS_OK;
#endif
}

das_result_t das_eth_link_state(das_eth_t eth, das_eth_link_state_t* state) {
    if (!das_eth_is_valid(eth) || state == 0) {
        return DAS_ERROR_INVALID_ARGUMENT;
    }

    state->up = false;
    state->speed_mbps = 0u;
    state->duplex = DAS_ETH_DUPLEX_UNKNOWN;

#if !defined(CORE_CM7)
    return DAS_ERROR_UNSUPPORTED;
#else
    if (!g_initialized || g_phy_address == UINT8_MAX) {
        return DAS_ERROR_NOT_READY;
    }
    return read_link_state(state);
#endif
}
