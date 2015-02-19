all: host

host: host.cpp gui.cpp iocore.cpp command.cpp *.h serial.o
	g++ -g -o host -I ./ host.cpp gui.cpp iocore.cpp command.cpp serial.o -L /opt/X11/lib -lpthread -lglut -lglu -lgl

serial.o: serial.c serial.h
	g++ -g -O3 -c -o serial.o serial.c -I ./

clean:
	rm -f serial.o host
