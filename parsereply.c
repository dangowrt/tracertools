#include "crc16.h"
#include <endian.h>
/* convert a reply to meaningful output */

#define CHG_FLAG_0 1
#define CHG_FLAG_1 2
#define CHG_FLAG_2 4

typedef struct reply {
	uint16_t	sync[3];
	uint8_t		addr;
	uint8_t		function;
	uint8_t		length;
	uint16_t	batv;
	uint16_t	panv;
	uint16_t	v1;
	uint16_t	loadc;
	uint16_t	v2;
	uint16_t	v3;
	uint8_t		loadon;
	uint8_t		b1;
	uint16_t	v4;
	uint8_t		b2;
	uint8_t		b3;
	uint16_t	panc;
	uint16_t	v5;
	uint8_t		chgflags;
	uint8_t		b4;
	uint16_t	crc;
	uint8_t		term;
	uint32_t	_pad1;
	uint32_t	_pad2;
	uint32_t	_pad3;
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

	if (fread(&r, 1, 37, stdin) < 36)
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

	batv = le16toh(r.batv);
	batv /= 100;

	panv = le16toh(r.panv);
	panv /= 100;

	panc = le16toh(r.panc);
	panc /= 100;

	loadc = le16toh(r.loadc);
	loadc /= 100;

	v1 = le16toh(r.v1);
	v2 = le16toh(r.v2);
	v3 = le16toh(r.v3);
	v4 = le16toh(r.v4);
	v5 = le16toh(r.v5);

	if (csvout) {
		printf("%.2f, %.2f, %.2f, %.2f, ", batv, panv, panc, loadc);
		printf("%d, %d, %d, %d, ", r.loadon, r.chgflags & CHG_FLAG_0,
			(r.chgflags & CHG_FLAG_1)>>1,
			(r.chgflags & CHG_FLAG_2)>>2);
		printf("%d, %d, %d, %d, %d, ", v1, v2, v3, v4, v5);
		printf("0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
			r.b1, r.b2, r.b3, r.b4);
	} else if (oneline) {
		printf("battery: %.2f V; ", batv);
		printf("panel: %.2f V * ", panv);
		printf("%.2f A = ", panc);
		printf("%.2f W; ", panv * panc);
		printf("load (%s): %.2f A; ", r.loadon?"on":"off", loadc);
		printf("charge mode %x\n", r.chgflags);
	} else {
		printf("battery voltage: %.2f V\n", batv);
		printf("panel voltage: %.2f V\n", panv);
		printf("panel current: %.2f A\n", panc);
		printf("panel work: %.2f W\n", panv * panc);
		printf("load current: %.2f A\n", loadc);
		printf("v1: %d\n", v1);
		printf("v2: %d\n", v2);
		printf("v3: %d\n", v3);
		printf("v4: %d\n", v4);
		printf("b1: %02x\n", r.b1);
		printf("b2: %02x\n", r.b2);
		printf("b3: %02x\n", r.b3);
		printf("b4: %02x\n", r.b4);
		printf("load power is %s\n", r.loadon?"on":"off");
		printf("charge mode is %x\n", r.chgflags);
	}
	return 0;
}
