#include "tracer_crc16.h"
/*
 * reqdata tool - 0.1
 * output data request package
 *
 * Copyright (C) 2014 Daniel Golle <daniel@makrotopia.org>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License as
 *      published by the Free Software Foundation, version 3.
 *
 */

uint8_t req[13] = { 0xeb, 0x90, 0xeb, 0x90, 0xeb, 0x90, /* sync */
			0x01, 0xa0, 0x01, /* head: address, function, length */
			0x03,		  /* data */
			0, 0,		  /* crc */
			0x7f };		  /* term */

int main(int args, char *argv[])
{
	unsigned int i;
	uint16_t crc_h, crc;

	if (args == 2) {
		req[7] = 0xaa;
		req[9] = atoi(argv[1]);
	}

	crc_h = tracer_crc16(&(req[6]), req[8]+5);
	crc = htobe16(crc_h);

	req[11] = (uint8_t)(crc & (uint16_t)0x00ff);
	req[10] = (uint8_t)(crc>>8 & (uint16_t)0x00ff);

	for(i=0;i<13;i++) {
		putc(req[i], stdout);
	}
	return 0;
}
