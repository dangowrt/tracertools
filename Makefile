all: tracerstat tracerreq

tracerstat: tracerstat.o

tracerreq:
	ln -s tracerstat tracerreq

clean:
	rm *.o tracerstat tracerreq

install:
