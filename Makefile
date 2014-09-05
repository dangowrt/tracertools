all: parsereply genreq

parsereply: parsereply.o

genreq:
	ln -s parsereply genreq

clean:
	rm *.o parsereply genreq

install:
