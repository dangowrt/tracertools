#include "crc16.h"

uint8_t req[13] = { 0xeb, 0x90, 0xeb, 0x90, 0xeb, 0x90, /* sync */
			0x01, 0xa0, 0x01, /* head: address, function, length */
			0x03,		  /* data */
			0xbd, 0xbb,	  /* crc */
			0x7f };		  /* term */

int main(int args, char *argv[])
{
	unsigned int i, co, cl;
	uint16_t crc, crcref;

	co = 9;
	cl = 1;
	crcref = (uint16_t)req[10] | (uint16_t)(req[11]) << 8;
	crc = crc16(&req[co], cl);
	if (crcref && crc != crcref) {
		fprintf(stderr, "please fix CRC calculation! either offsets or CRC algo itself don't match!\n");
		fprintf(stderr, "calculated CRC [%d:%d] doesn't match reference %04x != %04x\n", co, co+cl, crc, crcref);
		crc = crcref;
	}

	req[10] = (uint8_t)(crc & (uint16_t)0x00ff);
	req[11] = (uint8_t)(crc>>8 & (uint16_t)0x00ff);

	for(i=0;i<13;i++) {
		putc(req[i], stdout);
	}
	return 0;
}
