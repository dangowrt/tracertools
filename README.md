# Tracer solar charger probing/fuzzing #

This project intially started by reverse engineering the communication protocol
of the Tracer solar charge controller.

Fortunately [EP Solar](http://www.epsolarpv.com/) hinted to look at the
`Tracer-MT-5 protocol (111213).doc`
file and searching for that I found a Python and Arduino library for the
[Communication Interface for Tracer MT-5](https://github.com/xxv/tracer/).
Great to see there is more people hacking these controllers! And even got
something not entirely unlike a
[datasheet](https://github.com/xxv/tracer/blob/42e32a0e757e529d196cc04b29148ed4f442125e/docs/Protocol-Tracer-MT-5.pdf)

I made these two tools, initially meant to probe/fuzz and experiment with
captured communication logs. Now they are already useful to output the
status information of the Tracer and switch the load power on/off.

* `tracerreq` generate a request package. if called with a parameter, the load
output is switched according to the parameter (0/1). if called without a
parameter, a status-request is generated.

* `tracerstat` converts a captured status reply package to various useful
output formats.

Beware! This is hacky and crude prototype-quality software.

To query the status of your controller and output the result you can use socat
and stty from coreutils to set the baudrate.

    stty -F /dev/ttyUSB0 9600 raw
    tracerreq | socat - /dev/ttyUSB0,nonblock,raw,echo=0 | tracerstat

I use these tools in combination with socat to collect the status of a
Tracer MPPT solar controller:

    tmpfile="$( mktemp )"
    if [ -e "$tmpfile" ]; then
      stty -F /dev/ttyUSB0 9600 raw
      tracerreq | socat - /dev/ttyUSB0,nonblock,raw,echo=0 > "$tmpfile"
      tracerstat < "$tmpfile"
      tracerstat -o < "$tmpfile" | logger
      (
        echo -n "'$(uname -n)', '$(date)', "
        tracerstat -c < "$tmpfile"
      ) | alfred -s 160
      rm "$tmpfile"
    fi

The tracerstat tool also accepts parameters -c for CSV output and -o for a
human-readable one-liner (e.g. for use with syslog).

I also included some communication traces, so there is lots of sample material
with valid CRCs which should allow to find the CRC method used.


Thanks and have fun!
