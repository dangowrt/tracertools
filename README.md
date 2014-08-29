# Tracer solar charger probing/fuzzing #

To perform the needed reverse engineering tasks, I made these two tools:

* `reqdata` sends a captured data-request package, tries to recalculate CRC
and warns if it doesn't match the reference (good to test if the CRC
calculation works).

* `parsereply` converts a captured reply package to various useful output
formats.

* `fuzzreply [bit-to-flip]` sends a captured reply package, optionally flips a
bit and tries to recalculate the CRC.

The tools currently use the CRC-16 implementation I copy-pasted from
libmodbus...
This is obviously not entirely correct, so the right CRC calculation details
need to be found.

You can help by trying to fix crc16.h (and maybe reqdata.c to change offsets
and/or length of the CRC calculation input).
As currently the CRC calculation does not lead to the intended (sniffed)
results, changing and compiling these tools can allow users without access to
the hardware to have a guess ;)

To query the status of your controller and output the result you can use socat
and stty from coreutils to set the baudrate.

    stty -F /dev/ttyUSB0 9600 raw
    reqdata | socat - /dev/ttyUSB0,nonblock,raw,echo=0 | parsereply

I use these tools in combination with socat to collect the status of a
Tracer MPPT solar controller:

    tmpfile="$( mktemp )"
    if [ -e "$tmpfile" ]; then
      stty -F /dev/ttyUSB0 9600 raw
      reqdata | socat - /dev/ttyUSB0,nonblock,raw,echo=0 > "$tmpfile"
      parsereply < "$tmpfile"
      parsereply -o < "$tmpfile" | logger
      (
        echo -n "'$(uname -n)', '$(date)', "
        parsereply -c < "$tmpfile"
      ) | alfred -s 160
      rm "$tmpfile"
    fi

The parsereply tool also accepts parameters -c for CSV output and -o for a
human-readable one-liner (e.g. for use with syslog).

I also included some communication traces, so there is lots of sample material
with valid CRCs which should allow to find the CRC method used.


Thanks and have fun!
