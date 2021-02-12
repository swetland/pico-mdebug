// Copyright 2011-2021, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#ifndef _SWDP_H_
#define _SWDP_H_

void swd_init(void);
void swd_reset(uint32_t kind);
int swd_write(uint32_t reg, uint32_t val);
int swd_read(uint32_t reg, uint32_t *val);

unsigned swd_set_clock(unsigned khz);
unsigned swo_set_clock(unsigned khz);
void swd_hw_reset(int assert);

void jtag_init(void);
int jtag_io(unsigned count, unsigned tms, unsigned tdi, unsigned *tdo);

// swdp_read/write() register codes

// Park Stop Parity Addr3 Addr2 RnW APnDP Start

#define RD_IDCODE   0b10100101
#define RD_DPCTRL   0b10001101
#define RD_RESEND   0b10010101
#define RD_BUFFER   0b10111101

#define WR_ABORT    0b10000001
#define WR_DPCTRL   0b10101001
#define WR_SELECT   0b10110001
#define WR_BUFFER   0b10011001

#define RD_AP0      0b10000111
#define RD_AP1      0b10101111
#define RD_AP2      0b10110111
#define RD_AP3      0b10011111

#define WR_AP0      0b10100011
#define WR_AP1      0b10001011
#define WR_AP2      0b10010011
#define WR_AP3      0b10111011

#endif

