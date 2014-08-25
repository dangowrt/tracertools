all: fuzzreply parsereply reqdata

fuzzreply: fuzzreply.o

parsereply: parsereply.o

reqdata: reqdata.o

clean:
	rm *.o reqdata fuzzreply

install:
