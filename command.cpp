#include "command.h"
#include "host.h"
#include <climits>
using namespace std;

/* A command is COM_SIZE (currently 11) bytes. 
*  The first byte is the "opcode" signifying which command this is (move, wait, etc.)
*  The second byte is a command ID number.
*  Then come 8 bytes of data. Not all commands utilize all of this space.
*  Finally the last byte is the checksum, which is just the XOR of all of the first 10 bytes.
*/

/* ACHTUNG! multi-byte fields are stored in a command in BIG ENDIAN order, which is probably
* a poor choice considering most computers (including the Arduino) are little endian. But 
* that's the way I did it and it does work. Things could be made a little cleaner on the 
* firmware end by changing this, though on the host end it's good to set the individual bytes
* explicitly to maintain portability if this code is run on a big endian system */

// compute the checksum for 'c' and store it in its last byte
void checksum (command_t *c) {
	char cs = 0;
	for (int i=0; i < COM_SIZE-1; i++) {
		cs ^= c->bytes[i];
	}
	c->bytes[COM_SIZE-1] = cs;
}

/* These are various functions for initializing a command_t. They correspond to the various
*  arrangements of data that occur for different command types. */

// initialize a command with no data - just a opcode, id, and checksum.
command_t cmd_init (char op, char id) {
	command_t c;
	memset (c.bytes, 0, COM_SIZE);
	c.bytes[0] = op;
	c.bytes[1] = id;
	checksum (&c);
	return c;
}

// create a command with one byte of data
command_t cmd_initb (char op, char id, char b) {
	command_t c = cmd_init (op, id);
	c.bytes[2] = b;
	checksum (&c);
	return c;
}

// create a command with one 2-byte integer field. 
command_t cmd_inits (char op, char id, unsigned short x) {
	command_t c = cmd_init (op, id);
	c.bytes[2] = x >> 8;
	c.bytes[3] = x;
	checksum (&c);
	return c;
}

// create a command with two 2-byte integer fields.
command_t cmd_init2s (char op, char id, unsigned short x, unsigned short y) {
	command_t c = cmd_init (op, id);
	c.bytes[2] = x >> 8;
	c.bytes[3] = x;
	c.bytes[4] = y >> 8;
	c.bytes[5] = y;
	checksum (&c);
	return c;
}

/* create a command with four 2-byte fixed point (integer) fields computed
* from four floating point values. This compaction is probably unnecessary,
* though it does nearly halve the amount of data that must be transmitted over
* the serial port. 
*
* The floating point values are in mm (or mm/sec for feeds). To compute the fixed
* point values, multiply by 100 and convert to an integer. This gives a resolution
* of 0.01mm and a representable range of from 0 to 655.36mm (or from -327.68 to +327.67
* mm for relative moves)
*/
command_t cmd_init4f (char op, char id, float x, float y, float z, float f) {
	command_t c = cmd_init (op, id);
	uint16_t xi = (uint16_t) (x*100);
	uint16_t yi = (uint16_t) (y*100);
	uint16_t zi = (uint16_t) (z*100);
	uint16_t fi = (uint16_t) (f*100);
	c.bytes[2] = xi >> 8;
	c.bytes[3] = xi;
	c.bytes[4] = yi >> 8;
	c.bytes[5] = yi;
	c.bytes[6] = zi >> 8;
	c.bytes[7] = zi;
	c.bytes[8] = fi >> 8;
	c.bytes[9] = fi;
	checksum (&c);
	return c;
}

/* Now for the Gcode parsing. This will take a string and attempt to convert it
* into a list of command_t. */

int err = 0;	// a count of the number of errors on the most recent call to parse_gcode

// output a warning message to stderr and to the console in the GUI
void warning (int line, const char *message) {
	console_append (string("Warning! ") + message);
	fprintf (stderr, "Warning in line %d: %s\n", line, message);
	//err++;
}

// output an error message to stderr and to the console in the GUI
void error (int line, const char *message) {
	console_append (string("Error! ") + message);
	fprintf (stderr, "ERROR in line %d: %s\n", line, message);
	err++;
}

// the main parsing routine. It's a bit of a mess of pointer manipulation and 
// calls to C library routines with names with no vowels like strspn and strchr

// the Textscroller parameter is used to update the display of the currently 
// loaded gcode. If NULL is passed it will be ignored.
vector<command_t> parse_gcode (char *s, Textscroller *gcode) {
	err = 0;
	vector<command_t> cmd;

	float x, y, z, f;	// keeps track of the current position and feedrate
	x = y = z = f = 0;

	char *end = s + strlen(s);	// points to the end of the input string
	char *lbp;	// linebreak pointer - will point to the end of the current line
	int line = 0;
	uchar id = 0;
	do {
		line++;
		id = cmd.size();
		lbp = strchr (s, '\n');	// find the next newline
		if (lbp == NULL) lbp = end;	// if not found, set the end of the current line to the end of the input string

		if (gcode != NULL) {
			gcode->append_line (string (s, (size_t) (lbp-s)));
		}

		// now s points to start of line, lbp points to ending newline or null terminator
		s += strspn (s, " \t");	// skip whitespace
		if (*s == 'G') {
			long op = strtol (s+1, &s, 10);
			if (op == 1 || op == 2) {			// move
				bool absolute = op == 1;
				float tx = absolute ? x : 0;
				float ty = absolute ? y : 0;
				float tz = absolute ? z : 0;
				float tf = f;
				while (s != lbp && *s != ';') {
					s += strspn (s, " \t");
					char axis = *s;
					float val = strtof (s+1, &s);
					s += strspn (s, " \t");	// takes care of end-of-line whitespace
					printf ("axis = %c, val = %f\n", axis, val);
					if (val < 0) {
						error (line, "Negative numbers don't exist.");
					}
					switch (axis) {
						case 'X':
							tx = val;	break;
						case 'Y':
							ty = val;	break;
						case 'Z':
							tz = val;	break;
						case 'F':
							tf = val;	break;
						default:
							error (line, "Expecting either X#, Y#, Z#, or F# as arguments to G1 or G2");
					}
				}
				x = absolute ? tx : x + tx;
				y = absolute ? ty : y + ty;
				z = absolute ? tz : z + tz;
				f = tf;
				if (absolute) {
					cmd.push_back (cmd_init4f (MOVE, id, x, y, z, f));
				} else {
					cmd.push_back (cmd_init4f (RELMOVE, id, tx, ty, tz, f));
				}
			} else if (op == 4) {	// wait
				s = s + strspn (s, " \t");
				switch (*s) {
					case 'S':
						cmd.push_back (cmd_inits (WAIT, id, (uint16_t) (1000 * strtol (s+1, NULL, 10))));
						break;
					case 'P':
						cmd.push_back (cmd_inits (WAIT, id, (uint16_t) strtol (s+1, NULL, 10)));
						break;
					case 'M':
						cmd.push_back (cmd_initb (PAUSE, id, (uchar) strtol (s+1, NULL, 10)));
						break;
					default:
						error (line, "Expecting either S#, P#, or M# after G4 - wait");
				}

			} else if (op == 28) {	// home
				bool axes[3] = {false, false, false};
				char *q = s + strspn (s, " \t");
				if (q == lbp || *q == ';') {
					axes[0] = true;
					axes[1] = true;
					axes[2] = true;
				} else {
					while (*q >= 'X' && *q <= 'Z') {
						axes[*q - 'X'] = true;
						q++;
					}
				}
				uchar mask = 0;
				for (int i=0; i < 3; i++) {
					if (axes[i]) mask |= (1 << i);
				}
				cmd.push_back (cmd_initb (HOME, id, mask));

			} else if (op == 92) {	// set position
				float tx = x;
				float ty = y;
				float tz = z;
				while (s != lbp && *s != ';') {
					s += strspn (s, " \t");
					char axis = *s;
					float val = strtof (s+1, &s);
					s += strspn (s, " \t");	// takes care of end-of-line whitespace
					if (val < 0) {
						error (line, "Negative numbers don't exist.");
					}
					switch (axis) {
						case 'X':
							tx = val;	break;
						case 'Y':
							ty = val;	break;
						case 'Z':
							tz = val;	break;
						default:
							error (line, "Expecting either X#, Y#, or Z# as arguments to G92 - set position");
					}
				}
				x = tx;
				y = ty;
				z = tz;
				cmd.push_back (cmd_init4f (SET_POSITION, id, x, y, z, 0));
			} else {	//error
				error (line, "Unrecognized G#.");
			}

		} else if (*s == 'M') {
			long op = strtol (s+1, &s, 10);
			if (op == 0) {	// graceful stop
				cmd.push_back (cmd_initb (SPINDLE_ONOFF, id, 0));
				cmd.push_back (cmd_initb (HOME, (uchar) (id+1), 4));	// Z axis home
				cmd.push_back (cmd_initb (STEPPERS_ONOFF, (uchar) (id+2), 0));
				id += 2;
			} else if (op == 1) {	// echo
				command_t c = cmd_init (ECHO, id);
				s++;	// advance past the space.
				int i = 2;
				while (s < lbp && i < COM_SIZE) {
					c.bytes[i++] = *s;
					s++;
				}
				checksum (&c);
				cmd.push_back (c);

			} else if (op == 17 || op == 18) {	// enable/disable steppers
				cmd.push_back (cmd_initb (STEPPERS_ONOFF, id, 18-op));
			} else if (op == 112) {	// estop
				cmd.push_back (cmd_init (ESTOP, id));
			} else if (op == 114) {	// get position
				cmd.push_back (cmd_init (GET_POSITION, id));
			} else if (op == 119) {	// get endstop status
				cmd.push_back (cmd_init (GET_ENDSTOPS, id));
			} else if (op == 300) {	// beep
				int len = DEFAULT_BEEP_LEN;
				int freq = DEFAULT_BEEP_FREQ;
				s += strspn (s, " \t");	// skip whitespace
				while (s != lbp && *s != ';') {
					if (*s == 'S') {
						freq = (int) strtod (s+1, &s);
					} else if (*s == 'P') {
						len = (int) strtod (s+1, &s);
					} else {
						error (line, "Invalid argument to M300 - beep");
					}
					s += strspn (s, " \t");
				}
				if (freq > 15000) {
					warning (line, "Beep with frequency greater than 15000 Hz unsupported. Using 15 KHz.");
					freq = 15000;
				}
				cmd.push_back (cmd_init2s (BEEP, id, freq, len));
			} else {	//error
				error (line, "Unrecognized M-number.");
			}
		} else if (*s == ';') {	// semicolon indicates comments
		} else {
			if (lbp != end) {
				error (line, "Lines should start with 'G' or 'M'");
			}
		}
		s = lbp + 1;
	} while (lbp != end);

	return cmd;

}

void cmd_println (command_t c) {
	for (int i=0; i < COM_SIZE; i++) {
		printf ("%d ", c.bytes[i]);
	}
	printf ("\n");
}

string cmd_getstring (command_t c) {
	char s[50];
	char *ptr = s;
	for (int i=0; i < COM_SIZE; i++) {
		ptr += sprintf (ptr, "%d ", c.bytes[i]);
	}
	return string (s);
}
