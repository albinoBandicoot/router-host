#ifndef COMMAND_H
#define COMMAND_H

#include <vector>

#define COM_SIZE 11

typedef unsigned char uchar;
typedef struct {
	uchar bytes[COM_SIZE];
} command_t;

typedef enum {NOP, MOVE, HOME, STEPPERS_ONOFF, SPINDLE_ONOFF, WAIT, PAUSE, BEEP, SET_POSITION, GET_POSITION, GET_ENDSTOPS, GET_SPINDLE_SPEED, ECHO=16, ESTOP=255} comtype_t;

void cmd_println (command_t c);

std::vector<command_t> parse_gcode (char *);

#endif
