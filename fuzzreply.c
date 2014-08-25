#include "crc16.h"
/*
 * fuzzreply tool - 0.8
 * output captured reply, optionally flip a bit given by a parameter
 *
 * Copyright (C) 2014 Daniel Golle <daniel@makrotopia.org>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License as
 *      published by the Free Software Foundation, version 3.
 *
 */


/* a captured reply */
uint8_t reply[36] = { 0xeb, 0x90, 0xeb, 0x90, 0xeb, 0x90, /* sync */
		       0x00, 0xa0, 0x18, 	       /* head */
		       0xdc, 0x04, 0x50, 0x0d,	       /* data */
		       0x00, 0x00, 0x00, 0x00,
		       0x59, 0x04, 0x98, 0x05,
		       0x00, 0x00, 0x00, 0x29,
		       0x00, 0x00, 0x00, 0x01,
		       0x3a, 0x76, 0x01, 0x00,
		       0xd9, 0x69,		       /* crc */
		       0x7f  };			       /* term */


int main(int args, char *argv[])
{
	unsigned int i,x,y,b,co,cl;
	uint16_t crc,crcref;
	co = 9;
	cl = 24;

	if(args!=2) {
		crcref = (uint16_t)reply[33] | (uint16_t)(reply[34]) << 8;
		goto out;
	} else {
		crcref = 0;
	}

	/* read bit offset parameter */
	x = atoi(argv[1]);
	x += 9<<3;
	if(x >= 33<<3)
		return 2;

	/* flip bit b inside byte y */
	y = x>>3;
	b = x & 7;
	reply[y] ^= 1<<b;

	/* recalculate crc */
out:
	crc = crc16(&reply[co], cl);
	if (crcref && crc != crcref) {
		fprintf(stderr, "please fix CRC calculation! either offsets or CRC algo itself don't match!\n");
		fprintf(stderr, "calculated CRC [%d:%d] doesn't match reference %04x != %04x\n", co, co+cl, crc, crcref);
		crc = crcref;
	}

	reply[33] = (uint8_t)(crc & (uint16_t)0x00ff);
	reply[34] = (uint8_t)(crc>>8 & (uint16_t)0x00ff);

	for(i=0;i<36;i++) {
		putc(reply[i], stdout);
	}
	return 0;
}
