/* command.h - defining command initializers and gcode parser */
#ifndef COMMAND_H
#define COMMAND_H

#include "config.h"
#include "gui.h"
#include <vector>
#include <string>

#define COM_SIZE 20

typedef unsigned char uchar;
typedef struct {
	uchar bytes[COM_SIZE];
} command_t;

//typedef enum {NOP, MOVE, RELMOVE, HOME, STEPPERS_ONOFF, SPINDLE_ONOFF, WAIT, PAUSE, BEEP, SET_POSITION, GET_POSITION, GET_ENDSTOPS, GET_SPINDLE_SPEED, ECHO=16, ESTOP=255} comtype_t;
typedef enum {NOOP, MOVA, MOVR, MARC, MHLX, HOME, CLWO, SWOX, SWOY, CROT, SROT, EDGX, EDGY, EFMX, EFMY, EF2X, EF2Y, STPE, STPD, SPNE, SPND, SSPS, WAIT, WUSR, BEEP, QPOS, QABS, QWOR, QROT, QEND, QSPS, ECHO, STOP=255} comtype_t;

void cmd_println (command_t c);
string cmd_getstring (command_t c);

std::vector<command_t> parse_gcode (char *, Textscroller *);

command_t cmd_init   (char op, unsigned short id);
command_t cmd_initb  (char op, unsigned short id, char b);
command_t cmd_inits  (char op, unsigned short id, unsigned short);
command_t cmd_init2s (char op, unsigned short id, unsigned short, unsigned short);
command_t cmd_initf  (char op, unsigned short id, float);
command_t cmd_init3fb (char op, unsigned short id, float a, float b, float c, char d);
command_t cmd_init4f (char op, unsigned short id, float, float, float, float);


#endif
