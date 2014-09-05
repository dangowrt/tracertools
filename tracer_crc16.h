/*
 * A direct copy of the CRC function in the documentation
 * Protocol-Tracer-MT-5.pdf (PROTOCOL OF TRACER SEREIS AND MT-5  Ver 3)
 * found on https://github.com/xxv/tracer/blob/master/c/tracer_crc.c
 * modified by Daniel Golle <daniel@makrotopia.org>
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

uint16_t tracer_crc16(uint8_t *buf, uint16_t len) {
	uint8_t crc_i, crc_j, r1, r2, r3, r4;
	uint16_t crc;

	if (len < 2)
		return 0xdead;

	r1 = *buf;
	buf++;

	r2 = *buf;
	buf++;

	for (crc_i = 0; crc_i < len - 2; crc_i++) {
		r3 = *buf;
		buf++;

		for (crc_j=0; crc_j < 8; crc_j++) {
			r4 = r1;
			r1 = (r1<<1);

			if ((r2 & 0x80) != 0) {
				r1++;
			}

			r2 = r2<<1;

			if((r3 & 0x80) != 0) {
				r2++;
			}

			r3 = r3<<1;

			if ((r4 & 0x80) != 0) {
				r1 = r1^0x10;
				r2 = r2^0x41;
			}
		}
	}

	crc = r1;
	crc = crc << 8 | r2;

	return(crc);
}
