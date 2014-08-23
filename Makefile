all: fuzzreply reqdata

fuzzreply: fuzzreply.o

reqdata: reqdata.o

clean:
	rm *.o reqdata fuzzreply
