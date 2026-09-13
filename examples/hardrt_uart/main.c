// SPDX-License-Identifier: Apache-2.0

#include <das/das.h>
#include <hardrt.h>

#include <stdint.h>

#define TASK_STACK_WORDS 512u

static uint32_t g_led_stack[TASK_STACK_WORDS] __attribute__((aligned(8)));
static uint32_t g_uart_stack[TASK_STACK_WORDS] __attribute__((aligned(8)));
static das_uart_t g_console;
static uint32_t g_core_hz;

volatile uint32_t g_das_hardrt_led_count;
volatile uint32_t g_das_hardrt_uart_count;
volatile uint32_t g_das_hardrt_last_ms;

uint32_t hrt_port_get_core_hz(void) {
    return g_core_hz;
}

static das_time_ms_t hardrt_time_source(void* context) {
    (void)context;
    return hrt_now_ms();
}

static int das_init(void) {
    if (das_clock_set_frequency(UINT32_C(400000000)) != DAS_OK) {
        return 1;
    }
    if (das_clock_get_core_frequency(&g_core_hz) != DAS_OK || g_core_hz == 0u) {
        return 2;
    }
    if (das_board_led_init(DAS_BOARD_LED_GREEN, false) != DAS_OK) {
        return 3;
    }

    const das_uart_config_t uart_config = {
        .baud_rate = UINT32_C(115200),
        .data_bits = DAS_UART_DATA_BITS_8,
        .parity = DAS_UART_PARITY_NONE,
        .stop_bits = DAS_UART_STOP_BITS_1,
    };
    if (das_board_uart_init(DAS_BOARD_UART_STLINK_VCP,
                            &uart_config,
                            &g_console) != DAS_OK) {
        return 4;
    }

    return 0;
}

static int rtos_init(void) {
    const hrt_config_t rtos_config = {
        .tick_hz = UINT32_C(1000),
        .policy = HRT_SCHED_PRIORITY_RR,
        .default_slice = 5u,
        .core_hz = g_core_hz,
        .tick_src = HRT_TICK_SYSTICK,
    };
    if (hrt_init(&rtos_config) != HRT_OK) {
        return 5;
    }

    if (das_time_set_source(hardrt_time_source, 0) != DAS_OK) {
        return 6;
    }

    return 0;
}

static void led_task(void* arg) {
    (void)arg;

    for (;;) {
        if (das_board_led_toggle(DAS_BOARD_LED_GREEN) != DAS_OK) {
            hrt_task_delete();
            return;
        }
        ++g_das_hardrt_led_count;
        hrt_sleep(250u);
    }
}

static void uart_task(void* arg) {
    (void)arg;
    static const uint8_t message[] = "HardRT + DAS alive\r\n";

    for (;;) {
        if (das_uart_write_timeout(g_console,
                                   message,
                                   sizeof(message) - 1u,
                                   50u) != DAS_OK) {
            hrt_task_delete();
            return;
        }
        g_das_hardrt_last_ms = das_time_now_ms();
        ++g_das_hardrt_uart_count;
        hrt_sleep(1000u);
    }
}

int main(void) {
    int status = das_init();
    if (status != 0) {
        return status;
    }

    status = rtos_init();
    if (status != 0) {
        return status;
    }

    const hrt_task_attr_t led_attr = {
        .priority = HRT_PRIO1,
        .timeslice = 5u,
    };
    const hrt_task_attr_t uart_attr = {
        .priority = HRT_PRIO0,
        .timeslice = 5u,
    };

    if (hrt_create_task(led_task,
                        0,
                        g_led_stack,
                        TASK_STACK_WORDS,
                        &led_attr) < 0) {
        return 7;
    }
    if (hrt_create_task(uart_task,
                        0,
                        g_uart_stack,
                        TASK_STACK_WORDS,
                        &uart_attr) < 0) {
        return 8;
    }

    return hrt_start() == HRT_OK ? 0 : 9;
}
