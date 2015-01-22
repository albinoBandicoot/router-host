//
// arduino-serial-lib -- simple library for reading/writing serial ports
//
// 2006-2013, Tod E. Kurt, http://todbot.com/blog/
//

#ifndef SERIAL_H
#define SERIAL_H
#include <stdint.h>
//#include <jni.h>
//#include <JSerial.h>

int serialport_init(const char* serialport, int baud);
int serialport_close( int fd );
int serialport_writebyte( int fd, uint8_t b);
int serialport_write(int fd, void* str, int len);
int serialport_read (int fd, char *b);
int serialport_read_until(int fd, char* buf, char until, int buf_max, int timeout);
int serialport_flush(int fd);

#endif
