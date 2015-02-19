/* command.h - defining command initializers and gcode parser */
#ifndef COMMAND_H
#define COMMAND_H

#include "config.h"
#include "gui.h"
#include <vector>
#include <string>

#define COM_SIZE 11

typedef unsigned char uchar;
typedef struct {
	uchar bytes[COM_SIZE];
} command_t;

typedef enum {NOP, MOVE, RELMOVE, HOME, STEPPERS_ONOFF, SPINDLE_ONOFF, WAIT, PAUSE, BEEP, SET_POSITION, GET_POSITION, GET_ENDSTOPS, GET_SPINDLE_SPEED, ECHO=16, ESTOP=255} comtype_t;

void cmd_println (command_t c);
string cmd_getstring (command_t c);

std::vector<command_t> parse_gcode (char *, Textscroller *);

command_t cmd_init   (char op, char id);
command_t cmd_initb  (char op, char id, char b);
command_t cmd_inits  (char op, char id, unsigned short);
command_t cmd_init2s (char op, char id, unsigned short, unsigned short);
command_t cmd_init4f (char op, char id, float, float, float, float);


#endif
