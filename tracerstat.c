/*
 * tracer stat tool - 0.9
 * converts a status reply to meaningful output formats,
 * generates status request and load power switch requests.
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

/* a captured status request message with the CRC 0'd out */
uint8_t req[13] = { 0xeb, 0x90, 0xeb, 0x90, 0xeb, 0x90, /* sync */
			0x01, 0xa0, 0x01, /* head: address, function, length */
			0x03,		  /* data */
			0, 0,		  /* crc */
			0x7f };		  /* term */

typedef struct reply {
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
}__attribute__((packed)) reply_t;

/* a captured reply message */
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


/* generate request message */
int genreq(int args, char *argv[])
{
	unsigned int i;
	uint16_t crc_h, crc;

	/* power-switch if parameter is given */
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

/* parse status-reply message */
int parsereply(int args, char *argv[])
{
	uint8_t n,s;
	reply_t *r;
	uint8_t csvout = 0, oneline = 0;
	double batv, minv, maxv, panv, loadc, panc, pvc, flowc, batl, batf;
	int8_t temp;
	uint8_t buf[64];

	uint8_t const sync[] = { 0xeb, 0x90 };
	uint16_t crc, crc_n;

	if (args>2)
		return 1;

	if (args == 2 && strncmp(argv[1], "-c", 3) == 0)
		csvout = 1;

	if (args == 2 && strncmp(argv[1], "-o", 3) == 0)
		oneline = 1;

	if (fread(buf, 1, sizeof(buf), stdin) < sizeof(reply_t) + 2)
		return -1; /* EOPEN */

	/* sync-match */
	s = 0;
	for (n=0; n<12; n++) {
		if (s && ( n%2 != s - 1 ) )
			continue;

		if ( buf[n] == sync[0] && buf[n+1] == sync[1] )
			s = 1 + n % 2;
		else if ( s )
			break;
	}

	if (!s)
		return -2;

	/* if there was at least one sync pattern, set the msg pointer */
	r = (reply_t *)(&buf[n]);

	if (r->addr != 0)
		return -3; /* EINVAL */

	if (r->function != 160)
		return -3; /* EINVAL */

	if (r->length != 24)
		return -3; /* EINVAL */

	if (r->term != 127)
		return -2; /* EDATA */

	crc = tracer_crc16(&(r->addr), r->length + 5);
	if (crc) {
		fprintf(stderr, "crc error");
		return -2;
	}

	batv = le16toh(r->batv);
	batv /= 100;

	minv = le16toh(r->minv);
	minv /= 100;

	maxv = le16toh(r->maxv);
	maxv /= 100;

	panv = le16toh(r->panv);
	panv /= 100;

	loadc = le16toh(r->loadc);
	loadc /= 100;

	pvc = le16toh(r->pvc);
	pvc /= 100;

	temp = r->temp - 30;

	flowc = pvc - loadc;
	batl = ( 100 * ( batv - minv ) ) / ( maxv - minv );
	batf = ( r->batfull ? ((flowc>0)?0:flowc) : flowc ) * batv;

	if (csvout) {
		printf("%.2f, %.2f, %.2f, %.2f, ", batv, panv, pvc, loadc);
		printf("%.2f, %.2f, %d, ", minv, maxv, temp);
		printf("%d, %d, %d, %d, ", r->loadon, r->charging, r->overload, r->fuse);
		printf("%d, %d, %d, ", r->overdischarge, r->batfull, r->batoverload);
		printf("0x%02x, 0x%02x \n", r->b1, r->b2);
	} else if (oneline) {
		printf("battery: %.1f%%%s; ", batl, r->batfull?" (full)":"");
		printf("load: %s; flow: %+.2f W; ", r->loadon?"on":"off", batf);
		printf("t: %d degC; ", temp);
		printf("%s%s%s%s\n", r->overload?" overload!":"",
			r->fuse?" short-circuit!":"",
			r->batoverload?" battery overload!":"",
			r->overdischarge?" over discharge!":"");
	} else {
		printf("pv voltage: %.2f V\n", panv);
		printf("battery voltage: %.2f V\n", batv);
		printf("pwm current: %.2f A\n", pvc);
		printf("load power is %s\n", r->loadon?"on":"off");
		printf("load current: %.2f A\n", loadc);
		printf("battery flow: %+.2f W\n", batf);
		printf("battery level: %.1f%%\n", batl);
		printf("temperature: %d deg C\n", temp);
		printf("alarms:");
		printf("%s%s%s%s%s%s",
			r->batfull?" battery full.":"",
			r->overload?" overload!":"",
			r->fuse?" short-circuit!":"",
			r->batoverload?" battery overload!":"",
			r->overdischarge?" over discharge!":"",
			r->charging?"":" not charging!");
		printf("\n");

	}
	return 0;
}

int main(int args, char *argv[]) {
	char *cmd;

	cmd = basename(argv[0]);

	if (!strcmp(cmd, "tracerreq"))
		return genreq(args, argv);
	else if (!strcmp(cmd, "tracerstat"))
		return parsereply(args, argv);
	else
		return -1;
}
