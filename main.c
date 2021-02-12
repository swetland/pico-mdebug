// Copyright 2021, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#include <stdio.h>
#include <stdint.h>

#include <pico/stdlib.h>
#include <tusb.h>

void rswd_init();
uint32_t rswd_txn(uint32_t* rx, uint32_t rxc, uint32_t* tx);

uint32_t rxbuf[8192 / 4];
uint32_t txbuf[8192 / 4];

uint32_t rxcount = 0;

void mdebug_task(void) {
	if (!tud_vendor_available()) {
		return;
	}

	unsigned count = tud_vendor_read(rxbuf + rxcount, 64);
	if (count == 0) {
		return;
	}
	//printf("USB RX %u (%u)\n", count, rxcount);

	if (count & 3) {
		// bogus packet
		rxcount = 0;
		return;
	}

	rxcount += (count / 4);

	if (count < 64) {
		// short packet: end of txn
		count = rswd_txn(rxbuf, rxcount, txbuf);
		if (count) {
			tud_vendor_write(txbuf, count * 4);
		}
		rxcount = 0;
		return;
	}

	if (rxcount == (8192 / 4)) {
		// overflow
		rxcount = 0;
		return;
	}
}

int main() {
	board_init();
	setup_default_uart();
	printf("Hello, world!\n");
	rswd_init();
	tusb_init();

	for (;;) {
		tud_task();
		mdebug_task();
	}

	return 0;
}


static const tusb_desc_device_t dev_desc = {
	.bLength = sizeof(tusb_desc_device_t),
	.bDescriptorType = TUSB_DESC_DEVICE,
	.bcdUSB = 0x0110,
	.bDeviceClass = 0x00,
	.bDeviceSubClass = 0x00,
	.bDeviceProtocol = 0x00,
	.bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
	.idVendor = 0x1209,
	.idProduct = 0x5038,
	.bcdDevice = 0x0100,
	.iManufacturer = 0,
	.iProduct = 0,
	.iSerialNumber = 0,
	.bNumConfigurations = 1,
};

const uint8_t* tud_descriptor_device_cb(void) {
	return (const uint8_t*) &dev_desc;
}

#define IFC_MDEBUG 0
#define IFC_TOTAL 1

#define EP_MDEBUG_IN 0x81
#define EP_MDEBUG_OUT 0x01

#define CFG_LEN (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)

static const uint8_t cfg_desc[] = {
	TUD_CONFIG_DESCRIPTOR(1, IFC_TOTAL, 0, CFG_LEN,
		TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
	TUD_VENDOR_DESCRIPTOR(IFC_MDEBUG, 0, EP_MDEBUG_OUT, EP_MDEBUG_IN, 64),
};

const uint8_t* tud_descriptor_configuration_cb(uint8_t index) {
	return cfg_desc;
}

const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
	return NULL;
}
