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
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#define CACHE_PATH_PREFIX "/var/cache/tracerstat"
#define CACHE_LIFETIME 20 /* 20 seconds */

#define REQ_STATUS 0
#define REQ_PON 1
#define REQ_POFF 2

#define OUTFMT_ONELINE 0
#define OUTFMT_VERBOSE 1
#define OUTFMT_CSV 2
#define OUTFMT_JSON 4

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


void *genstatefn(char *fn, char *devid) {
	snprintf(fn, 64, "%s-%s", CACHE_PATH_PREFIX, devid);
}

int try_open_cache(int outdated, char *devid) {
	int fd;
	struct stat cachestat;
	char statefilename[64];
	genstatefn(statefilename, devid);
	fd = open(statefilename, O_RDONLY);
	if (fd < 0)
		return -1;
	if (fstat(fd, &cachestat))
		return -1;

	if (!outdated && difftime(time(NULL), cachestat.st_mtime) > (double)2) {
		close(fd);
		return -2;
	}

	return fd;
}

void invalidate_cache(char *devid) {
	struct stat cachestat;
	char statefilename[64];
	genstatefn(statefilename, devid);
	unlink(statefilename);
}

/* send request message */
int sendreq(int fd, unsigned int reqtype)
{
	unsigned int i;
	uint16_t crc_h, crc;
	/* a captured status request message with the CRC 0'd out */
	uint8_t req[13] = { 0xeb, 0x90, 0xeb, 0x90, 0xeb, 0x90, /* sync */
			0x01, 0xa0, 0x01, /* head: address, function, length */
			0x03,		  /* data */
			0, 0,		  /* crc */
			0x7f };		  /* term */


	/* power-switch if parameter is given */
	if (reqtype != REQ_STATUS) {
		req[7] = 0xaa;
		req[9] = reqtype & REQ_PON;
	}

	crc_h = tracer_crc16(&(req[6]), req[8]+5);
	crc = htobe16(crc_h);

	req[11] = (uint8_t)(crc & (uint16_t)0x00ff);
	req[10] = (uint8_t)(crc>>8 & (uint16_t)0x00ff);

	if (write(fd, req, 13) < 13)
		return -2;

	return 0;
}


/* read and parse reply message */
int readreply(int fd, int outformat, char *devid, int nocache)
{
	fd_set readfs;
	uint8_t n, s;
	reply_t *r;
	uint8_t csvout = 0, oneline = 0, jsonout = 0, p = 0;
	double batv, minv, maxv, panv, loadc, panc, pvc, flowc, batl, batf;
	int8_t temp, l = -1;
	uint8_t buf[64];
	int res;
	struct timeval tout;

	uint8_t const sync[] = { 0xeb, 0x90 };
	uint16_t crc, crc_n;

	csvout = outformat == OUTFMT_CSV;
	jsonout = outformat == OUTFMT_JSON;
	oneline = !(outformat & OUTFMT_VERBOSE);

	FD_SET(fd, &readfs);
	tout.tv_usec = 500000;  /* milliseconds */
	tout.tv_sec = 0;  /* seconds */
	while (l < 25) {
		res = select(fd+1, &readfs, NULL, NULL, &tout);
		if (!res)
			return -2; /* reply timeout */

		if (FD_ISSET(fd, &readfs))
			l = read(fd, buf, sizeof(buf));
	}

	/* sync-match */
	s = 0;
	for (n=0; n<12; n++) {
		if (s && ( n%2 != s - 1 ))
			continue;

		if (buf[n] == sync[0] && buf[n+1] == sync[1])
			s = 1 + n % 2;
		else if (s)
			break;
	}

	if (!s)
		return -2;

	/* if there was at least one sync pattern, set the msg pointer */
	r = (reply_t *)(&buf[n]);

	if (r->addr != 0)
		return -3; /* EINVAL */

	if (r->length > 24)
		return -3;

	crc = tracer_crc16(&(r->addr), r->length + 5);
	if (crc) {
		return -2;
	}

	if (r->function == 170) {
		if (csvout) {
			printf("%d", buf[n+3]);
		} else if (jsonout) {
			printf("{ \"class\": \"sensor\", \"load_switch\": %d }", buf[n+3]);
		} else {
			printf("load power switched %s", buf[n+3]?"on":"off");
			if (!oneline)
				printf("\n");
		}
		invalidate_cache(devid);
		return 0;
	}

	if (r->function != 160)
		return -3; /* EINVAL */

	if (r->length != 24)
		return -3; /* EINVAL */

	if (r->term != 127)
		return -2; /* EDATA */

	/* store result in cache */
	if (!nocache) {
		char *tmpfilename = "/tmp/tracerstatXXXXXX";
		int outfd;
		outfd = mkstemp(tmpfilename);
		if (outfd > 0) {
			char statefilename[64];
			genstatefn(statefilename, devid);
			write(outfd, buf, l);
			close(outfd);
			rename(tmpfilename, statefilename);
		}
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
		printf("0x%02x, 0x%02x", r->b1, r->b2);
	} else if (jsonout) {
		printf("{ \"class\": \"sensors\", \"bat_volt\": %.2f, ", batv);
		printf("\"bat_volt_min\": %.2f, \"bat_volt_max\": %.2f, ", minv, maxv);
		printf("\"in1_volt\": %.2f, \"in1_amp\": %.2f, ", panv, pvc);
		printf("\"in2_amp\": %.2f, \"temp1\": %.2f, ", loadc, temp);
		printf("\"chg_state\": \"%s\", \"load_switch_state\": \"%s\"",
				r->batfull?"batful":r->charging?"charging":"idle",
				r->loadon?"on":"off");
		if (r->overload || r->fuse || r->batoverload || r->overdischarge) {
			printf("\"alarms\" : {");
			if (r->overload) {
				printf("%s%s", p?", ":"", "\"overload\"");
				p=1;
			}
			if (r->fuse) {
				printf("%s%s", p?", ":"", "\"shortcircuit\"");
				p=1;
			}
			if (r->batoverload) {
				printf("%s%s", p?", ":"", "\"batoverload\"");
				p=1;
			}
			if (r->overdischarge) {
				printf("%s%s", p?", ":"", "\"overdischarge\"");
			}
			printf(" }");
		}
		printf(" }");
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

int open_tracer(char *device) {
	struct termios mode;
	int fd;
	fd = open(device, O_RDWR | O_NONBLOCK);
	if (fd < 0 || !isatty(fd))
		return -2;

	tcgetattr(fd, &mode);
	mode.c_iflag = 0;
	mode.c_oflag &= ~OPOST;
	mode.c_lflag &= ~(ISIG | ICANON);
	mode.c_cc[VMIN] = 1;
	mode.c_cflag |= CS8;
	if (cfsetispeed(&mode, B9600) < 0 || cfsetospeed(&mode, B9600) < 0)
		return -2;

	tcsetattr(fd, TCSANOW, &mode);

	return fd;
}

int main(int args, char *argv[]) {
	int argn, outfmt = OUTFMT_VERBOSE, reqtype = REQ_STATUS;
	char *device = "/dev/ttyUSB0";
	char *devid;
	int dev_fd = -1, cache_fd = -1;

	for (argn = 1; argn < args; argn++) {
		if (!strncmp(argv[argn], "-o", 3))
			outfmt &= ~OUTFMT_VERBOSE;
		else if (!strncmp(argv[argn], "-c", 3))
			outfmt = OUTFMT_CSV | outfmt & OUTFMT_VERBOSE;
		else if (!strncmp(argv[argn], "-j", 3))
			outfmt = OUTFMT_JSON;
		else if (!strncmp(argv[argn], "-I", 3))
			reqtype = REQ_PON;
		else if (!strncmp(argv[argn], "-O", 3))
			reqtype = REQ_POFF;
		else
			device = argv[argn];
	}

	devid = basename(device);

	/* try cache */
	if (reqtype == REQ_STATUS) {
		int res;
		cache_fd = try_open_cache(0, devid);
		if (cache_fd > 0) {
			res = readreply(cache_fd, outfmt, devid, 1);
			close(cache_fd);
			if (!res)
				return 0;
		};
	}

	/* actually query device */
	dev_fd = open_tracer(device);
	if (dev_fd < 0) {
		fprintf(stderr, "can't open device %s\n", device);
		return -1;
	}
	sendreq(dev_fd, reqtype);
	readreply(dev_fd, outfmt, devid, 0);
	close(dev_fd);
}
