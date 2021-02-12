/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// based on picoprobe's probe.c

//#include <pico/stdlib.h>
//#include <stdio.h>
//#include <string.h>

#include <hardware/clocks.h>
#include <hardware/gpio.h>

#include "swd-io.pio.h"

#define DIV_ROUND_UP(m, n)	(((m) + (n) - 1) / (n))

#define SWDIO_SM 0
#define PIN_OFFSET 2
#define PIN_SWCLK (PIN_OFFSET + 0)
#define PIN_SWDIO (PIN_OFFSET + 1)

static uint32_t swdio_offset;

unsigned swdio_set_freq(unsigned freq_khz) {
    uint clk_sys_freq_khz = clock_get_hz(clk_sys) / 1000;
    // Worked out with saleae
    uint32_t divider = clk_sys_freq_khz / freq_khz / 2;
    pio_sm_set_clkdiv_int_frac(pio0, SWDIO_SM, divider, 0);
    return clk_sys_freq_khz / divider / 2;
}

static inline void swdio_write_bits(uint32_t data, unsigned bit_count) {
    pio_sm_put_blocking(pio0, SWDIO_SM, bit_count - 1);
    pio_sm_put_blocking(pio0, SWDIO_SM, data);
    // Wait for pio to push garbage to rx fifo so we know it has finished sending
    pio_sm_get_blocking(pio0, SWDIO_SM);
}

static inline uint32_t swdio_read_bits(unsigned bit_count) {
    pio_sm_put_blocking(pio0, SWDIO_SM, bit_count - 1);
    uint32_t data = pio_sm_get_blocking(pio0, SWDIO_SM);
    if (bit_count < 32) {
        data >>= (32 - bit_count);
    }
    return data;
}

static void swdio_read_mode(void) {
    pio_sm_exec(pio0, SWDIO_SM, pio_encode_jmp(swdio_offset + swdio_offset_in_posedge));
    while(pio0->dbg_padoe & (1 << PIN_SWDIO));
}

static void swdio_write_mode(void) {
    pio_sm_exec(pio0, SWDIO_SM, pio_encode_jmp(swdio_offset + swdio_offset_out_negedge));
    while(!(pio0->dbg_padoe & (1 << PIN_SWDIO)));
}

void swdio_init() {
    // Funcsel pins
    pio_gpio_init(pio0, PIN_SWCLK);
    pio_gpio_init(pio0, PIN_SWDIO);

    // Make sure SWDIO has a pullup on it. Idle state is high
    gpio_pull_up(PIN_SWDIO);

    swdio_offset = pio_add_program(pio0, &swdio_program);

    pio_sm_config sm_config = swdio_program_get_default_config(swdio_offset);
    
    // Set SWCLK as a sideset pin
    sm_config_set_sideset_pins(&sm_config, PIN_SWCLK);

    // Set SWDIO offset
    sm_config_set_out_pins(&sm_config, PIN_SWDIO, 1);
    sm_config_set_set_pins(&sm_config, PIN_SWDIO, 1);
    sm_config_set_in_pins(&sm_config, PIN_SWDIO);

    // Set SWD and SWDIO pins as output to start. This will be set in the sm
    pio_sm_set_consecutive_pindirs(pio0, SWDIO_SM, PIN_OFFSET, 2, true);

    // shift output right, autopull off, autopull threshold
    sm_config_set_out_shift(&sm_config, true, false, 0);
    // shift input right as swd data is lsb first, autopush off
    sm_config_set_in_shift(&sm_config, true, false, 0);

    // Init SM with config
    pio_sm_init(pio0, SWDIO_SM, swdio_offset, &sm_config);

    // Set up divisor
    swdio_set_freq(4000);

    // Enable SM
    pio_sm_set_enabled(pio0, SWDIO_SM, 1);

    // Jump to write program
    swdio_write_mode();
}
