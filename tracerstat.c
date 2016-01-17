/*
 * tracer stat tool - 0.9
 * generates status and load power switch requests and converts the
 * reply to meaningful output formats.
 *
 * Copyright (C) 2014 Daniel Golle <daniel@makrotopia.org>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License as
 *      published by the Free Software Foundation, version 3.
 *
 */

#include "tracer_crc16.h"
#include <string.h>
#include <libgen.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define DEFAULT_DEVICE "/dev/ttyUSB0"
#define CACHE_PATH_PREFIX "/var/spool/tracerstat"
#define CACHE_LIFETIME 2 /* seconds */

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

int open_cache(int outdated, char *devid) {
	int fd;
	struct stat cachestat;
	char statefilename[64];
	genstatefn(statefilename, devid);
	fd = open(statefilename, O_RDONLY | O_NONBLOCK );
	if (fd < 0)
		return -1;

	if (fstat(fd, &cachestat))
		return -1;

	if (!outdated && difftime(time(NULL), cachestat.st_mtime) > (double)CACHE_LIFETIME) {
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


/* todo: check and use UUCP-style lockfiles in /var/lock/ */
int open_tracer(char *device) {
	struct termios mode;
	struct stat devstat;
	int fd;

	fd = open(device, O_RDWR | O_NONBLOCK | O_NOCTTY);

	if (fd < 0)
		return -1;

	if (fstat(fd, &devstat))
		return -1;

	if(!isatty(fd))
		return -1;

	tcgetattr(fd, &mode);
	mode.c_iflag = 0;
	mode.c_oflag &= ~OPOST;
	mode.c_lflag &= ~(ISIG | ICANON);
	mode.c_cc[VMIN] = 1;
	mode.c_cc[VTIME] = 0;
	mode.c_cflag |= CS8;
	if (cfsetispeed(&mode, B9600) < 0 || cfsetospeed(&mode, B9600) < 0)
		return -2;

	tcflush(fd, TCIOFLUSH);
	tcsetattr(fd, TCSANOW, &mode);
	tcflush(fd, TCIOFLUSH);

	return fd;
}

/* send request message */
int sendreq(int fd, unsigned int reqtype)
{
	unsigned int i;
	uint16_t crc;
	/* a captured status request message with the CRC 0'd out */
	uint8_t req[19] = { 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, /* pwl start up */
			0xeb, 0x90, 0xeb, 0x90, 0xeb, 0x90, /* sync */
			0x01, 0xa0, 0x01, /* head: address, function, length */
			0x03,		  /* data */
			0, 0,		  /* crc */
			0x7f };		  /* term */


	/* power-switch if parameter is given */
	if (reqtype != REQ_STATUS) {
		req[13] = 0xaa;
		req[15] = reqtype & REQ_PON;
	}

	crc = tracer_crc16(&(req[12]), req[14]+5);

	req[17] = (uint8_t)(crc & (uint16_t)0x00ff);
	req[16] = (uint8_t)(crc>>8 & (uint16_t)0x00ff);

	if (write(fd, req, 19) < 19)
		return -2;

	return 0;
}


/* read and parse reply message */
int readreply(int fd, int outformat, unsigned char nocache, char *devid)
{
	fd_set readfs;
	uint8_t n, s;
	reply_t *r;
	uint8_t csvout = 0, oneline = 0, jsonout = 0, p = 0, l = 0;
	uint32_t batv, minv, maxv, panv, loadc, panc, pvc;
	int8_t temp;
	uint8_t buf[64] = {0,};
	int timeout = 0, res = 0;
	struct timeval tout;
	unsigned char istty;

	uint8_t const sync[] = { 0xeb, 0x90 };
	uint16_t crc, crc_n;

	csvout = !!(outformat & OUTFMT_CSV);
	jsonout = !!(outformat & OUTFMT_JSON);
	oneline = !(outformat & OUTFMT_VERBOSE);

	FD_SET(fd, &readfs);
	tout.tv_usec = 500000;
	tout.tv_sec = 0;
	istty = isatty(fd);
	s = istty?0:3;
	timeout = !s;
	do {
		if (istty)
			timeout = !select(fd+1, &readfs, NULL, NULL, &tout);
		if (!istty || FD_ISSET(fd, &readfs)) {
			/* read byte-wise until sync */
			res = read(fd, &buf[l], s==4?sizeof(buf) - l:s?s:1);
			if (res > 0) {
				l += res;
				if (l == 2 && buf[0] == sync[0] &&
				    buf[1] == sync[1]) {
					s = 2;
					l = 0;
					continue;
				} else if (l == 3 && buf[1] == sync[0] &&
				    buf[2] == sync[2]) {
					s = 1;
					l = 0;
					continue;
				} else if (s & 3 && l > 0 &&
					buf[0] != sync[0]) {
					s = 4; /* sync'ed */
				}

				if (s == 4 && l >= 5) {
					if (l >= buf[2] + 5)
						break;
				}
			}
		}
	} while (istty?!timeout:res>0);

	if (l < 5) /* smallest possible paket */
		return -2;

	if (!s)
		return -2;

	/* if there was at least one sync pattern, set the msg pointer */
	r = (reply_t *)(&buf);

	if (r->addr != 0)
		return -3; /* EINVAL */

	if (r->length > 24)
		return -3;

	crc = tracer_crc16(&(r->addr), r->length + 5);
	if (crc) {
		return -2;
	}

	if (r->function == 170) {
		if (r->length != 1)
			return -3; /* EINVAL */
		if (csvout) {
			printf("%d", buf[3]);
		} else if (jsonout) {
			printf("{ \"class\": \"sensor\", \"load_switch\": %d }", buf[3]);
		} else {
			printf("load power switched %s", buf[3]?"on":"off");
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

	/* store result in cache */
	if (!nocache) {
		char tmpfilename[32];
		int outfd;
		strncpy(tmpfilename, "/tmp/tracertemp-XXXXXX", 31);
		outfd = mkstemp(tmpfilename);
		if (outfd > 0) {
			char statefilename[64];
			genstatefn(statefilename, devid);
			write(outfd, buf, r->length + 5); /* skip EOM char */
			close(outfd);
			if (rename(tmpfilename, statefilename))
				unlink(tmpfilename);
		}
	}

	batv = le16toh(r->batv);
	batv *= 10;

	minv = le16toh(r->minv);
	minv *= 10;

	maxv = le16toh(r->maxv);
	maxv *= 10;

	panv = le16toh(r->panv);
	panv *= 10;

	loadc = le16toh(r->loadc);
	loadc *= 10;

	pvc = le16toh(r->pvc);
	pvc *= 10;

	temp = r->temp - 30;

	const char *collectd_hostname = getenv("COLLECTD_HOSTNAME");
	if (collectd_hostname) {
		printf("PUTVAL \"%s/tracer-0/voltage-battery\" U:%.2f\n", collectd_hostname, batv / 1000.0);
		printf("PUTVAL \"%s/tracer-0/voltage-panel\" U:%.2f\n", collectd_hostname, panv / 1000.0);
		printf("PUTVAL \"%s/tracer-0/current-mppt\" U:%.2f\n", collectd_hostname, pvc / 1000.0);
		printf("PUTVAL \"%s/tracer-0/current-load\" U:%.2f\n", collectd_hostname, loadc / 1000.0);
		printf("PUTVAL \"%s/tracer-0/temperature-system\" U:%d\n", collectd_hostname, temp);
	} else if (csvout) {
		printf("%u, %u, %u, %u, ", batv, panv, pvc, loadc);
		printf("%u, %u, %d, ", minv, maxv, temp);
		printf("%d, %d, %d, %d, ", r->loadon, r->charging, r->overload, r->fuse);
		printf("%d, %d, %d, ", r->overdischarge, r->batfull, r->batoverload);
		printf("0x%02x, 0x%02x", r->b1, r->b2);
	} else if (jsonout) {
		printf("{ \"class\": \"sensors\", \"name\": \"tracermppt\", \"bat_volt\": %u, ", batv);
		printf("\"bat_volt_min\": %u, \"bat_volt_max\": %u, ", minv, maxv);
		printf("\"in1_volt\": %u, \"in1_amp\": %u, ", panv, pvc);
		printf("\"in2_amp\": %u, \"temp1\": %d, ", loadc, temp);
		printf("\"chg_state\": \"%s\", \"load_switch\": %d",
			r->batfull?"batful":r->charging?"charging":"idle", r->loadon);
		if (r->overload || r->fuse || r->batoverload || r->overdischarge) {
			printf(", \"alarms\" : [");
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
			printf(" ]");
		}
		printf(" }");
	} else if (oneline) {
		printf("battery: %u mV%s; ", batv, r->batfull?" (full)":"");
		printf("load: %s; t: %d degC;", r->loadon?"on":"off", temp);
		printf("%s%s%s%s\n", r->overload?" overload!":"",
			r->fuse?" short-circuit!":"",
			r->batoverload?" battery overload!":"",
			r->overdischarge?" over discharge!":"");
	} else {
		printf("pv voltage: %u mV\n", panv);
		printf("battery voltage: %u mV\n", batv);
		printf("pwm current: %u mA\n", pvc);
		printf("load power is %s\n", r->loadon?"on":"off");
		printf("load current: %u mA\n", loadc);
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
	int argn, outfmt = OUTFMT_VERBOSE, reqtype = REQ_STATUS;
	char device[64] = {0,};
	char *devid;
	int dev_fd = -1, cache_fd = -1;
	int res;
	unsigned int tries = 0;
	unsigned char use_cache = 1;

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
		else if (!strncmp(argv[argn], "-R", 3))
			use_cache = 0;
		else
			strncpy(device, argv[argn], sizeof(device));
	}

	if (!device[0])
		strncpy(device, DEFAULT_DEVICE, sizeof(device));

	devid = basename(device);

	/* try cache */
	if (use_cache && reqtype == REQ_STATUS) {
		cache_fd = open_cache(0, devid);
		if (cache_fd > 0) {
			res = readreply(cache_fd, outfmt, 1, devid);
			close(cache_fd);
			if (res)
				invalidate_cache(devid);
			else
				return 0;
		};
	}

	do {
	/* actually query device */
	dev_fd = open_tracer(device);
		if (dev_fd < 0) {
			fprintf(stderr, "can't open device %s\n", device);
			return -1;
		}
		res = sendreq(dev_fd, reqtype);
		if (res) continue;
		res = readreply(dev_fd, outfmt, !use_cache, devid);
		if (!res) break;

		tcflush(dev_fd, TCIOFLUSH);
		close(dev_fd);
		usleep(50000);
		tries++;
	} while(tries < 7);

	return 0;
}
