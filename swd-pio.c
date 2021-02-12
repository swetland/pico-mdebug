// Copyright 2021, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "swd.h"
#include "rswdp.h"

#include "swd-pio.h"

/* returns 1 if the number of bits set in n is odd */
static unsigned parity(unsigned n) {
	n = (n & 0x55555555) + ((n & 0xaaaaaaaa) >> 1);
	n = (n & 0x33333333) + ((n & 0xcccccccc) >> 2);
	n = (n & 0x0f0f0f0f) + ((n & 0xf0f0f0f0) >> 4);
	n = (n & 0x00ff00ff) + ((n & 0xff00ff00) >> 8);
	n = (n & 0x0000ffff) + ((n & 0xffff0000) >> 16);
	return n & 1;
}

void swd_init(void) {
	swdio_init();
}

int swd_write(uint32_t hdr, uint32_t data) {
	unsigned timeout = 64;
	unsigned n;
	unsigned p = parity(data);
	
	if (hdr == WR_BUFFER) {
		// writing to TARGETSEL is done blind
		swdio_write_mode();
		swdio_write_bits(hdr << 8, 16);

		swdio_read_mode();
		swdio_read_bits(5);

		swdio_write_mode();
		swdio_write_bits(data, 32);
		swdio_write_bits(p, 1);
		return 0;
	}

	for (;;) {
		swdio_write_mode();
		swdio_write_bits(hdr << 8, 16);

		swdio_read_mode();
		n = swdio_read_bits(5);

		swdio_write_mode();

		// check the ack bits, ignoring the surrounding turnaround bits
		switch ((n >> 1) & 7) {
		case 1: // OK
			swdio_write_bits(data, 32);
			swdio_write_bits(p, 1);
			return 0;
		case 2: // WAIT
			if (--timeout == 0) {
				return ERR_TIMEOUT;
			}
			continue;
		default: // ERROR
			return ERR_IO;
		}
	}
}

int swd_read(uint32_t hdr, uint32_t *val) {
	unsigned timeout = 64;
	unsigned n, data, p;

	for (;;) {
		swdio_write_mode();
		swdio_write_bits(hdr << 8, 16);

		swdio_read_mode();
		n = swdio_read_bits(4);

		switch (n >> 1) {
		case 1: // OK
			data = swdio_read_bits(32);
			// read party + turnaround
			p = swdio_read_bits(2) & 1;
			if (p != parity(data)) {
				return ERR_PARITY;
			}
			*val = data;
			return 0;
		case 2: // WAIT
			swdio_read_bits(1);
			if (--timeout == 0) {
				return ERR_TIMEOUT;
			}
			continue;
		default: // ERROR
			swdio_read_bits(1);
			return ERR_IO;
		}
	}
}

void swd_reset(uint32_t kind) {
	swdio_write_mode();

	switch (kind) {
	case ATTACH_SWD_RESET:
		// 50+ HI (60 here), 2+ LO (4 here)
		swdio_write_bits(0xffffffff, 32);
		swdio_write_bits(0x0fffffff, 32);
		break;
	case ATTACH_JTAG_TO_SWD:
		swdio_write_bits(0xffffffff, 32);
		swdio_write_bits(0xffffffff, 32);
		swdio_write_bits(0b1110011110011110, 16);
		break;
	case ATTACH_DORMANT_TO_SWD:
		swdio_write_bits(0xffff, 16);
		swdio_write_bits(0x6209F392, 32);
		swdio_write_bits(0x86852D95, 32);
		swdio_write_bits(0xE3DDAFE9, 32);
		swdio_write_bits(0x19BC0EA2, 32);
		swdio_write_bits(0xF1A0, 16);
		break;
	case ATTACH_SWD_TO_DORMANT:
		swdio_write_bits(0xffffffff, 32);
		swdio_write_bits(0xffffffff, 32);
		swdio_write_bits(0xE3BC, 16);
		break;
	default:
	       break;
	}	     
}

unsigned swd_set_clock(unsigned khz) {
	return swdio_set_freq(khz);
}

unsigned swo_set_clock(unsigned khz) {
	return khz;
}

void swd_hw_reset(int assert) {
#if 0
    if (assert) {
        gpio_set(GPIO_RESET, 0);
        gpio_set(GPIO_RESET_TXEN, 1);
    } else {
        gpio_set(GPIO_RESET, 1);
        gpio_set(GPIO_RESET_TXEN, 0);
    }
#endif
}
