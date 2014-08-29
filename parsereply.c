/*
 * parsereply tool - 0.8
 * converts a reply to meaningful output formats
 *
 * Copyright (C) 2014 Daniel Golle <daniel@makrotopia.org>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License as
 *      published by the Free Software Foundation, version 3.
 *
 */

#include "crc16.h"
#include <endian.h>

#define CHG_FLAG_0 1
#define CHG_FLAG_1 2
#define CHG_FLAG_2 4

typedef struct reply {
	uint16_t	sync[3];
	uint8_t		addr; /* 0 = reply/broadcast */
	uint8_t		function; /* = 160 */
	uint8_t		length; /* = 24 */
	uint16_t	batv; /* battery voltage in 10mV steps */
	uint16_t	panv; /* panel voltage in 10mV steps */
	uint16_t	v1; /* always 0? */
	uint16_t	loadc; /* current on load port in 10 mA steps */
	uint16_t	v2; /* some voltage <batv, temperature? */
	uint16_t	v3; /* some voltage >=batv, charge target voltage? */
	uint8_t		loadon; /* 1 if load is powered on */
	uint8_t		b1; /* 0 */
	uint8_t		b2; /* 0 */
	uint8_t		b3; /* 3a..41 if dark/unconnected, 60.. sunshine */
	uint8_t		b4; /* 0 */
	uint8_t		b5; /* 0 */
	uint8_t		b6; /* 1 if daylight, 0 at night */
	uint8_t		b7; /* 1 if panel connected, 0 otherwise */
	uint16_t	v4; /* changes with amount of sun, current? */
	uint8_t		chgflags; /* 0 if not charging */
	uint8_t		b8; /* 0 */
	uint16_t	crc;
	uint8_t		term;
	uint32_t	_pad[7];
}__attribute__((packed)) reply_t;

/* { 0xeb, 0x90, 0xeb, 0x90, 0xeb, 0x90, */		/* sync */
/*		       0x00, 0xa0, 0x18, */		/* head */
/*		       0xdc, 0x04, 0x50, 0x0d, */	/* data */
/*		       0x00, 0x00, 0x00, 0x00,
		       0x59, 0x04, 0x98, 0x05,
		       0x00, 0x00, 0x00, 0x29,
		       0x00, 0x00, 0x00, 0x01,
		       0x3a, 0x76, 0x01, 0x00, */
/*		       0xd9, 0x69, */			/* crc */
/*		       0x7f  }; */			/* term */


int main(int args, char *argv[])
{
	uint8_t n;
	reply_t r;
	uint8_t csvout = 0, oneline = 0;
	double batv, panv, loadc, panc;
	uint16_t v1, v2, v3, v4, v5;
	uint16_t const sync = 0x90eb;

	if (args>2)
		return 1;

	if (args == 2 && strncmp(argv[1], "-c", 3) == 0)
		csvout = 1;

	if (args == 2 && strncmp(argv[1], "-o", 3) == 0)
		oneline = 1;

	if (fread(&r, 1, 64, stdin) < 36)
		return -1; /* EOPEN */

	for(n=0;n<3;n++) {
		if (le16toh(r.sync[n]) != sync)
			return -2; /* EDATA */
	}

	if (r.addr != 0)
		return -3; /* EINVAL */
	
	if (r.function != 160)
		return -3; /* EINVAL */

	if (r.length != 24)
		return 4; /* ENOIMPL */

	if (r.term != 127)
		return -2; /* EDATA */

	/* should do a CRC check, but CRC details need to be discovered */

	batv = le16toh(r.batv); /* ok */
	batv /= 100;

	panv = le16toh(r.panv); /* ok */
	panv /= 100;

	loadc = le16toh(r.loadc); /* ok */
	loadc /= 100;

	v1 = le16toh(r.v1);
	v2 = le16toh(r.v2);
	v3 = le16toh(r.v3);
	v4 = le16toh(r.v4);

	if (csvout) {
		printf("%.2f, %.2f, %.2f, %.2f, ", batv, panv, loadc);
		printf("%d, %d, %d, %d, ", r.loadon, r.chgflags & CHG_FLAG_0,
			(r.chgflags & CHG_FLAG_1)>>1,
			(r.chgflags & CHG_FLAG_2)>>2);
		printf("%d, %d, %d, %d, ", v1, v2, v3, v4);
		printf("0x%02x, 0x%02x, 0x%02x, 0x%02x, ", r.b1, r.b2, r.b3, r.b4);
		printf("0x%02x, 0x%02, 0x%02x, 0x%02x\n", r.b5, r.b6, r.b7, r.b8);
	} else if (oneline) {
		printf("battery: %.2f V; ", batv);
		printf("panel: %.2f V * ", panv);
		printf("load (%s): %.2f A; ", r.loadon?"on":"off", loadc);
		printf("charge mode %x\n", r.chgflags);
	} else {
		printf("battery voltage: %.2f V\n", batv);
		printf("panel voltage: %.2f V\n", panv);
		printf("charge mode is %x\n", r.chgflags);
		printf("load power is %s\n", r.loadon?"on":"off");
		printf("load current: %.2f A\n", loadc);
		printf("v1: %d\n", v1);
		printf("v2: %d\n", v2);
		printf("v3: %d\n", v3);
		printf("v4: %d\n", v4);
		printf("b1: %02x\n", r.b1);
		printf("b2: %02x\n", r.b2);
		printf("b3: %02x\n", r.b3);
		printf("b4: %02x\n", r.b4);
		printf("b5: %02x\n", r.b5);
		printf("b6: %02x\n", r.b6);
		printf("b7: %02x\n", r.b7);
		printf("b8: %02x\n", r.b8);
	}
	return 0;
}
