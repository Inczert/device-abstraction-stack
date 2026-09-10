// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>

volatile uint32_t g_das_link_test_data = UINT32_C(0x13579bdf);
volatile uint32_t g_das_link_test_bss;

int main(void) {
    g_das_link_test_bss = g_das_link_test_data;

    for (;;) {
        __asm volatile("nop");
    }
}
