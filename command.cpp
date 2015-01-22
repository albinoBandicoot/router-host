#include "command.h"
#include <climits>
using namespace std;

void checksum (command_t *c) {
	char cs = 0;
	for (int i=0; i < COM_SIZE-1; i++) {
		cs ^= c->bytes[i];
	}
	c->bytes[COM_SIZE-1] = cs;
}

command_t cmd_init (char op, char id) {
	command_t c;
	c.bytes[0] = op;
	c.bytes[1] = id;
	checksum (&c);
	return c;
}

command_t cmd_initb (char op, char id, char b) {
	command_t c = cmd_init (op, id);
	c.bytes[2] = b;
	checksum (&c);
	return c;
}

command_t cmd_inits (char op, char id, unsigned short x) {
	command_t c = cmd_init (op, id);
	c.bytes[2] = x >> 8;
	c.bytes[3] = x;
	checksum (&c);
	return c;
}

command_t cmd_init2s (char op, char id, unsigned short x, unsigned short y) {
	command_t c = cmd_init (op, id);
	c.bytes[2] = x >> 8;
	c.bytes[3] = x;
	c.bytes[4] = y >> 8;
	c.bytes[5] = y;
	checksum (&c);
	return c;
}

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

void error (int line, const char *message) {
	fprintf (stderr, "ERROR in line %d: %s\n", line, message);
}

vector<command_t> parse_gcode (char *s) {
	vector<command_t> cmd;

	float x, y, z, f;
	x = y = z = f = 0;

	char *ssave = s;
	char *end = s + strlen(s);
	char *lbp;
	int line = 0;
	uchar id = 0;
	do {
		line++;
		id = cmd.size();
		lbp = strchr (s, '\n');
		if (lbp == NULL) lbp = end;

		// now s points to start of line, lbp points to ending newline or null terminator
		s += strspn (s, " \t");	// skip whitespace
		if (*s == 'G') {
			long op = strtol (s+1, &s, 10);
			if (op == 1) {			// move
				float tx = x;
				float ty = y;
				float tz = z;
				float tf = f;
				while (s != lbp) {
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
							error (line, "Expecting either X#, Y#, Z#, or F# as arguments to G1 - move");
					}
				}
				x = tx;
				y = ty;
				z = tz;
				f = tf;
				cmd.push_back (cmd_init4f (MOVE, id, x, y, z, f));

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
			} else {	//error
			}
		} else if (*s == ';') {
		} else {
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
