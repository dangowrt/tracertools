all: parsereply reqdata

parsereply: parsereply.o

reqdata: reqdata.o

clean:
	rm *.o reqdata parsereply

install:
