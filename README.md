= Tracer solar charger probing/fuzzing =

To perform the needed reverse engineering tasks, I made these two tools:

* reqdata
Sends a captured data-request package, tries to recalculate CRC and warns if
it doesn't match the reference (good to test if the CRC calculation works).

* fuzzreply [bit-to-flip]
Sends a captured reply package, optionally flips a bit and tries to
recalculate the CRC.

Both currently use the CRC-16 implementation I copy-pasted from libmodbus...
This is obviously not entirely correct, so the right CRC calculation details
need to be found.
You can help by trying to fix crc16.h (and maybe reqdata.c to change offsets
and/or length of the CRC calculation input).
As currently the CRC calculation does not lead to the intended (sniffed)
results, changing and compiling these tools can allow users without access to
the hardware to have a guess ;)


I used them in combination with socat, in order to get a status from the
controller you can run:

    stty -F /dev/ttyUSB0 9600 raw
    ./reqdata | socat - /dev/ttyUSB0,nonblock,raw,echo=0 | hexdump -C


I also included some communication traces, so there is lots of sample material.


Thanks and have fun!
