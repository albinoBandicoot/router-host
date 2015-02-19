#ifndef CONFIG_H
#define CONFIG_H

#define BAUDRATE 9600	// must match setting in firmware
#define SERIAL_PORT_NAME "/dev/tty.usbmodem1421"	// ideally this should be selectable at runtime

#define SP_SPEED_MIN 0
#define SP_SPEED_MAX 12000

#define DEFAULT_BEEP_LEN 400	// milliseconds
#define DEFAULT_BEEP_FREQ 800	// Hz

// strings to display on the speed selection buttons. 
#define SLOW_FEEDRATE_STR "0.5 mm/sec"
#define MED_FEEDRATE_STR "4 mm/sec"
#define FAST_FEEDRATE_STR "20 mm/sec"

// if you're nice, you'll set these feedrates to correspond to the strings above.
const float FEEDRATES[3] = {0.5f, 4, 20};

/* GUI CONFIGURATION */
#define DEFAULT_WIN_XS 800
#define DEFAULT_WIN_YS 600

#define TS_LINE_HT 18	// height in pixels of a line of text in a Textscroller component
#define MIN_SCROLLBAR_HT 15	// minimum size of the scrollbar

// what to display in the console
#define CONSOLE_ACK true
#define CONSOLE_PING true
#define CONSOLE_SEND true
#define CONSOLE_RESP true

#endif
