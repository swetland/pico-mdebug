// Copyright 2011-2021, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "swd.h"
#include "rswdp.h"

void usb_xmit(void *data, unsigned len);
int usb_recv(void *data, unsigned len);

unsigned swdp_trace = 0;

// indicates host knows about v1.0 protocol features
unsigned host_version = 0;

static uint8_t optable[16] = {
    [OP_RD | OP_DP | OP_X0] = RD_IDCODE,
    [OP_RD | OP_DP | OP_X4] = RD_DPCTRL,
    [OP_RD | OP_DP | OP_X8] = RD_RESEND,
    [OP_RD | OP_DP | OP_XC] = RD_BUFFER,
    [OP_WR | OP_DP | OP_X0] = WR_ABORT,
    [OP_WR | OP_DP | OP_X4] = WR_DPCTRL,
    [OP_WR | OP_DP | OP_X8] = WR_SELECT,
    [OP_WR | OP_DP | OP_XC] = WR_BUFFER,
    [OP_RD | OP_AP | OP_X0] = RD_AP0,
    [OP_RD | OP_AP | OP_X4] = RD_AP1,
    [OP_RD | OP_AP | OP_X8] = RD_AP2,
    [OP_RD | OP_AP | OP_XC] = RD_AP3,
    [OP_WR | OP_AP | OP_X0] = WR_AP0,
    [OP_WR | OP_AP | OP_X4] = WR_AP1,
    [OP_WR | OP_AP | OP_X8] = WR_AP2,
    [OP_WR | OP_AP | OP_XC] = WR_AP3,
};

static const char *board_str = "rpi pico";
static const char *build_str = "fw v1.00 (" __DATE__ ", " __TIME__ ")";

#define MODE_SWD    0
#define MODE_JTAG   1
static unsigned mode = MODE_SWD;

/* TODO bounds checking -- we trust the host far too much */
unsigned process_txn(uint32_t txnid, uint32_t *rx, int rxc, uint32_t *tx) {
    unsigned msg, op, n;
    unsigned txc = 1;
    unsigned count = 0;
    unsigned status = 0;
    void (*func)(void) = 0;

    tx[0] = txnid;

    while (rxc-- > 0) {
        count++;
        msg = *rx++;
        op = RSWD_MSG_OP(msg);
        n = RSWD_MSG_ARG(msg);
#if CONFIG_MDEBUG_TRACE
        printf("> %02x %02x %04x <\n", RSWD_MSG_CMD(msg), op, n);
#endif
        switch (RSWD_MSG_CMD(msg)) {
            case CMD_NULL:
                continue;
            case CMD_SWD_WRITE:
                while (n-- > 0) {
                    rxc--;
                    status = swd_write(optable[op], *rx++);
                    if (status) {
                        goto done;
                    }
                }
                continue;
            case CMD_SWD_READ:
                tx[txc++] = RSWD_MSG(CMD_SWD_DATA, 0, n);
                while (n-- > 0) {
                    status = swd_read(optable[op], tx + txc);
                    if (status) {
                        txc++;
                        while (n-- > 0)
                            tx[txc++] = 0xfefefefe;
                        goto done;
                    }
                    txc++;
                }
                continue;
            case CMD_SWD_DISCARD:
                while (n-- > 0) {
                    uint32_t tmp;
                    status = swd_read(optable[op], &tmp);
                    if (status) {
                        goto done;
                    }
                }
                continue;
            case CMD_ATTACH:
                if (mode != MODE_SWD) {
                    mode = MODE_SWD;
                    swd_init();
                }
                swd_reset(op);
                continue;
#if 0
            case CMD_JTAG_IO:
                if (mode != MODE_JTAG) {
                    mode = MODE_JTAG;
                    jtag_init();
                }
                tx[txc++] = RSWD_MSG(CMD_JTAG_DATA, 0, n);
                while (n > 0) {
                    unsigned xfer = (n > 32) ? 32 : n;
                    jtag_io(xfer, rx[0], rx[1], tx + txc);
                    rx += 2;
                    rxc -= 2;
                    txc += 1;
                    n -= xfer;
                }
                continue;
            case CMD_JTAG_VRFY:
                if (mode != MODE_JTAG) {
                    mode = MODE_JTAG;
                    jtag_init();
                }
                // (n/32) x 4 words: TMS, TDI, DATA, MASK
                while (n > 0) {
                    unsigned xfer = (n > 32) ? 32 : n;
                    jtag_io(xfer, rx[0], rx[1], tx + txc);
                    if ((tx[txc] & rx[3]) != rx[2]) {
                        status = ERR_BAD_MATCH;
                        goto done;
                    }
                    rx += 4;
                    rxc -= 4;
                    n -= xfer;
                }
                continue;
            case CMD_JTAG_TX: {
                unsigned tms = (op & 1) ? 0xFFFFFFFF : 0;
                if (mode != MODE_JTAG) {
                    mode = MODE_JTAG;
                    jtag_init();
                }
                while (n > 0) {
                    unsigned xfer = (n > 32) ? 32 : n;
                    jtag_io(xfer, tms, rx[0], rx);
                    rx++;
                    rxc--;
                    n -= xfer;
                }
                continue;
            }
            case CMD_JTAG_RX: {
                unsigned tms = (op & 1) ? 0xFFFFFFFF : 0;
                unsigned tdi = (op & 2) ? 0xFFFFFFFF : 0;
                if (mode != MODE_JTAG) {
                    mode = MODE_JTAG;
                    jtag_init();
                }
                tx[txc++] = RSWD_MSG(CMD_JTAG_DATA, 0, n);
                while (n > 0) {
                    unsigned xfer = (n > 32) ? 32 : n;
                    jtag_io(xfer, tms, tdi, tx + txc);
                    txc++;
                    n -= xfer;
                }
                continue;
            }
#endif
            case CMD_RESET:
                swd_hw_reset(n);
                continue;
            case CMD_TRACE:
                swdp_trace = op;
                continue;
#if 0
            case CMD_BOOTLOADER:
                func = _reboot;
                continue;
#endif
            case CMD_SET_CLOCK:
                n = swd_set_clock(n);
                if (host_version >= RSWD_VERSION_1_0) {
                    tx[txc++] = RSWD_MSG(CMD_CLOCK_KHZ, 0, n);
                }
                continue;
            case CMD_SWO_CLOCK:
                n = swo_set_clock(n);
                if (host_version >= RSWD_VERSION_1_0) {
                    tx[txc++] = RSWD_MSG(CMD_CLOCK_KHZ, 1, n);
                }
                continue;
            case CMD_VERSION:
                host_version = n;
                tx[txc++] = RSWD_MSG(CMD_VERSION, 0, RSWD_VERSION);

                n = strlen(board_str);
                memcpy(tx + txc + 1, board_str, n + 1);
                n = (n + 4) / 4;
                tx[txc++] = RSWD_MSG(CMD_BOARD_STR, 0, n);
                txc += n;

                n = strlen(build_str);
                memcpy(tx + txc + 1, build_str, n + 1);
                n = (n + 4) / 4;
                tx[txc++] = RSWD_MSG(CMD_BUILD_STR, 0, n);
                txc += n;

                tx[txc++] = RSWD_MSG(CMD_RX_MAXDATA, 0, 8192);
                txc += n;
                continue;
            default:
                printf("unknown command %02x\n", RSWD_MSG_CMD(msg));
                status = 1;
                goto done;
        }
    }

done:
    tx[txc++] = RSWD_MSG(CMD_STATUS, status, count);

    /* if we're about to send an even multiple of the packet size
     * (64), add a NULL op on the end to create a short packet at
     * the end.
     */
    if ((txc & 0xf) == 0)
        tx[txc++] = RSWD_MSG(CMD_NULL, 0, 0);

#if CONFIG_MDEBUG_TRACE
    printf("[ send %d words ]\n", txc);
    for (n = 0; n < txc; n+=4) {
        printx("%08x %08x %08x %08x\n",
               tx[n], tx[n+1], tx[n+2], tx[n+3]);
    }
#endif

    return txc;
}

void rswd_init(void) {
	printf("[ rswdp agent v0.9 ]\n");
	printf("[ built " __DATE__ " " __TIME__ " ]\n");

	swd_init();
}

uint32_t rswd_txn(uint32_t* rx, uint32_t rxc, uint32_t* tx) {
#if CONFIG_MDEBUG_TRACE
	printf("[ recv %d words ]\n", rxc);
	for (unsigned n = 0; n < rxc; n += 4) {
		printf("%08x %08x %08x %08x\n",
			rx[n], rx[n+1], rx[n+2], rx[n+3]);
        }
#endif
       
	if ((rx[0] & 0xFFFF0000) != 0xAA770000) {
		printf("invalid frame %x\n", rx[0]);
		return 0;
	}

        return process_txn(rx[0], rx + 1, rxc - 1, tx);
}

