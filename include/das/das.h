// SPDX-License-Identifier: Apache-2.0
#ifndef DAS_DAS_H
#define DAS_DAS_H

/*
 * Convenience umbrella for the application-facing DAS C API.
 *
 * Architecture-specific startup/vector contracts remain explicit and are not
 * pulled in here. Include <das/cortex_m/startup.h> only when an application
 * deliberately needs to customize Cortex-M startup or vector ownership.
 */
#include <das/result.h>
#include <das/clock.h>
#include <das/time.h>
#include <das/irq.h>
#include <das/gpio.h>
#include <das/cache.h>
#include <das/dma.h>
#include <das/uart.h>
#include <das/spi.h>
#include <das/i2c.h>
#include <das/timer.h>
#include <das/eth.h>
#include <das/board.h>
#include <das/board_resources.h>

#endif /* DAS_DAS_H */
