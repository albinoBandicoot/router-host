#ifndef CONFIG_H
#define CONFIG_H

#define BAUDRATE 9600
#define SERIAL_PORT_NAME "/dev/tty.usbmodem1421"

#define SP_SPEED_MIN 0
#define SP_SPEED_MAX 12000

#define DEFAULT_BEEP_LEN 400	// milliseconds
#define DEFAULT_BEEP_FREQ 800	// Hz

#define SLOW_FEEDRATE_STR "0.5 mm/sec"
#define MED_FEEDRATE_STR "4 mm/sec"
#define FAST_FEEDRATE_STR "20 mm/sec"

const float FEEDRATES[3] = {0.5f, 4, 20};

/* GUI CONFIGURATION */
#define DEFAULT_WIN_XS 800
#define DEFAULT_WIN_YS 600

#define TS_LINE_HT 18
#define MIN_SCROLLBAR_HT 15

// what to display in the console
#define CONSOLE_ACK true
#define CONSOLE_PING true
#define CONSOLE_SEND true
#define CONSOLE_RESP true

#endif
