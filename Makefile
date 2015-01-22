all: host

host: host.cpp iocore.cpp command.cpp *.h serial.o
	g++ -O3 -o host -I ./ host.cpp iocore.cpp command.cpp serial.o

serial.o: serial.c serial.h
	g++ -O3 -c -o serial.o serial.c -I ./

clean:
	rm -f serial.o host
