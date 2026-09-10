// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_STM32H755_DMA_INTERNAL_H
#define DAS_STM32H755_DMA_INTERNAL_H

#include <stdint.h>

#include <das/dma.h>

/* STM32H755 DMAMUX1 request identifiers. Kept below the public DAS API. */
#define STM32H755_DMA_REQUEST_MEM2MEM UINT32_C(0)
#define STM32H755_DMA_REQUEST_SPI1_RX UINT32_C(37)
#define STM32H755_DMA_REQUEST_SPI1_TX UINT32_C(38)

das_result_t stm32h755_dma_set_request(das_dma_t dma, uint32_t request);

#endif
