# Tracer solar charger probing/fuzzing #

To perform the needed reverse engineering tasks, I made these two tools.

Fortunately EP Solar hinted to look at the `Tracer-MT-5 protocol (111213).doc`
file and searching for that I found https://github.com/xxv/tracer/
Great to see there is more people hacking these controllers! And even got
something not entirely unlike a datasheet.

Beware! This is hacky and crude prototype-quality software.

* `genreq` generate a request package. if called with a parameter, the load
output is switched according to the parameter. if called without a parameter,
a status-request is generated.

* `parsereply` converts a captured status reply package to various useful
output formats.

To query the status of your controller and output the result you can use socat
and stty from coreutils to set the baudrate.

    stty -F /dev/ttyUSB0 9600 raw
    genreq | socat - /dev/ttyUSB0,nonblock,raw,echo=0 | parsereply

I use these tools in combination with socat to collect the status of a
Tracer MPPT solar controller:

    tmpfile="$( mktemp )"
    if [ -e "$tmpfile" ]; then
      stty -F /dev/ttyUSB0 9600 raw
      genreq | socat - /dev/ttyUSB0,nonblock,raw,echo=0 > "$tmpfile"
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
