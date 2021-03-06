# Tracer solar charger monitoring and control tools #

## (on Linux and possible other POSIX-compatible operating systems) ##

This project intially started by reverse engineering the communication protocol
of the Tracer solar charge controller.

Fortunately [EP Solar](http://www.epsolarpv.com/) hinted to look at the
`Tracer-MT-5 protocol (111213).doc`
file and searching for that I found a Python and Arduino library for the
[Communication Interface for Tracer MT-5](https://github.com/xxv/tracer/).
Great to see there is more people hacking these controllers! And even got
something not entirely unlike a
[datasheet](https://github.com/xxv/tracer/blob/42e32a0e757e529d196cc04b29148ed4f442125e/docs/Protocol-Tracer-MT-5.pdf)

I initially made two tools written in C, meant to probe/fuzz and experiment
with captured communication logs. Now that became a single tool which is useful
to output the status information of the Tracer and switch the load power on/off.

`tracerstat` allows sending MT-5 (pseudo-ModBus) requests via a serial port
to the device and translates the result into various useful output formats.

  * `-o` one-line output (no newline, useful for syslog)
  * `-c` comma seperated output
  * `-j` JSON output

`tracerstat` automatically outputs in [collectd](http://collectd.org/)'s expected format if called
from collectd's [Exec](https://github.com/collectd/collectd/blob/master/src/collectd.conf.in#L523) plugin.
```
LoadPlugin exec
<Plugin exec>
	Exec "daemon:uucp" "/usr/bin/tracerstat" "/dev/ttyUSB1"
</Plugin>
```

When called with parameter `-I` or `-O`, the load output is switched according
to the parameter.

To query the status of your controller on the command line, call `tracerstat`
without any parameters assuming that the controller is connected to your host
via /dev/ttyUSB0.

If using a serial port device other than `/dev/ttyUSB0`, simply put the device
node filename as a paramter.

Thanks and have fun!
