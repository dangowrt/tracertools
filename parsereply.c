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

#include "tracer_crc16.h"
#include <endian.h>
#include <string.h>
#include <libgen.h>

uint8_t req[13] = { 0xeb, 0x90, 0xeb, 0x90, 0xeb, 0x90, /* sync */
			0x01, 0xa0, 0x01, /* head: address, function, length */
			0x03,		  /* data */
			0, 0,		  /* crc */
			0x7f };		  /* term */

int main_genreq(int args, char *argv[])
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


typedef struct reply {
	uint16_t	sync[3];
	uint8_t		addr; /* 0 = reply/broadcast */
	uint8_t		function; /* = 160 */
	uint8_t		length; /* = 24 */
	uint16_t	batv; /* battery voltage in 10mV steps */
	uint16_t	panv; /* panel voltage in 10mV steps */
	uint16_t	v1; /* always 0? */
	uint16_t	loadc; /* current on load port in 10 mA steps */
	uint16_t	minv; /* battery min voltage */
	uint16_t	maxv; /* battery full voltage */
	uint8_t		loadon; /* 1 if load is powered on */
	uint8_t		overload; /* 1 if overload */
	uint8_t		fuse; /* 1 if load short-circuit */
	uint8_t		b1; /* 3a..41 if dark/unconnected, 60.. sunshine, reserved? */
	uint8_t		batoverload; /* battery overload */
	uint8_t		overdischarge; /* 1 if over-discharge */
	uint8_t		batfull; /* 1 if full */
	uint8_t		charging; /* 1 if charging, 0 otherwise */
	uint8_t		temp; /* -30 ? */
	uint16_t	pvc; /* pv current */
	uint8_t		b2; /* 0 reserved*/
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


int main_parsereply(int args, char *argv[])
{
	uint8_t n;
	reply_t r;
	uint8_t csvout = 0, oneline = 0;
	double batv, minv, maxv, panv, loadc, panc, pvc, flowc, batl;
	int8_t temp;

	uint16_t const sync = 0x90eb;
	uint16_t crc, crc_n;

	if (args>2)
		return 1;

	if (args == 2 && strncmp(argv[1], "-c", 3) == 0)
		csvout = 1;

	if (args == 2 && strncmp(argv[1], "-o", 3) == 0)
		oneline = 1;

	if (fread(&r, 1, sizeof(reply_t), stdin) < 36)
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
		return -3; /* EINVAL */

	if (r.term != 127)
		return -2; /* EDATA */

	crc = tracer_crc16(&(r.addr), r.length + 5);
	if (crc) {
		fprintf(stderr, "crc error");
		return -2;
	}

	batv = le16toh(r.batv); /* ok */
	batv /= 100;

	minv = le16toh(r.minv); /* ok */
	minv /= 100;

	maxv = le16toh(r.maxv); /* ok */
	maxv /= 100;

	panv = le16toh(r.panv); /* ok */
	panv /= 100;

	loadc = le16toh(r.loadc); /* ok */
	loadc /= 100;

	pvc = le16toh(r.pvc); /* ok */
	pvc /= 100;

	temp = r.temp - 30;

	flowc = pvc - loadc;
	batl = ( 100 * ( batv - minv ) ) / ( maxv - minv );

	if (csvout) {
		printf("%.2f, %.2f, %.2f, %.2f, ", batv, panv, pvc, loadc);
		printf("%.2f, %.2f, %d, ", minv, maxv, temp);
		printf("%d, %d, %d, %d, ", r.loadon, r.charging, r.overload, r.fuse);
		printf("%d, %d, %d, ", r.overdischarge, r.batfull, r.batoverload);
		printf("0x%02x, 0x%02x \n", r.b1, r.b2);
	} else if (oneline) {
		printf("battery: %.1f%%%s; ", batl, r.batfull?" (full)":"");
		printf("load: %s; flow: %+.2f W; ", r.loadon?"on":"off", flowc * batv);
		printf("t: %d degC; ", temp);
		printf("%s%s%s%s\n", r.overload?" overload!":"",
			r.fuse?" short-circuit!":"",
			r.batoverload?" battery overload!":"",
			r.overdischarge?" over discharge!":"");
	} else {
		printf("pv voltage: %.2f V\n", panv);
		printf("battery voltage: %.2f V\n", batv);
		printf("pwm current: %.2f A\n", pvc);
		printf("load power is %s\n", r.loadon?"on":"off");
		printf("load current: %.2f A\n", loadc);
		printf("flow: %+.2f W\n", flowc * batv);
		printf("battery level: %.1f%%\n", batl);
		printf("temperature: %d deg C\n", temp);
		printf("alarms:");
		printf("%s%s%s%s%s", r.overload?" overload!":"",
			r.fuse?" short-circuit!":"",
			r.batoverload?" battery overload!":"",
			r.overdischarge?" over discharge!":"",
			r.charging?"":" not charging!");
		printf("\n");

	}
	return 0;
}

int main(int args, char *argv[]) {
	char *cmd;

	cmd = basename(argv[0]);

	if (!strcmp(cmd, "genreq") || !strcmp(cmd, "reqdata"))
		return main_genreq(args, argv);
	else if (!strcmp(cmd, "parsereply"))
		return main_parsereply(args, argv);
	else
		return -1;
}
