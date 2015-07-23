#include "command.h"
#include "host.h"
#include <climits>
using namespace std;

/* A command is COM_SIZE (currently 20) bytes. 
*  The first byte is the "opcode" signifying which command this is (move, wait, etc.)
*  The next two bytes form a command ID number, big endian.
*  Then come 16 bytes of data. Not all commands utilize all of this space.
*  Finally the last byte is the checksum, which is just the XOR of all of the first 19 bytes.
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

void getfloat (uchar *p, float f) {
	uchar *src = (uchar *) &f;
	p[0] = src[3];
	p[1] = src[2];
	p[2] = src[1];
	p[3] = src[0];
}

/* These are various functions for initializing a command_t. They correspond to the various
*  arrangements of data that occur for different command types. */

// initialize a command with no data - just a opcode, id, and checksum.
command_t cmd_init (char op, unsigned short id) {
	command_t c;
	memset (c.bytes, 0, COM_SIZE);
	c.bytes[0] = op;
	c.bytes[1] = (uchar) id >> 8;
	c.bytes[2] = (uchar) id;
	checksum (&c);
	return c;
}

// create a command with one byte of data
command_t cmd_initb (char op, unsigned short id, char b) {
	command_t c = cmd_init (op, id);
	c.bytes[3] = b;
	checksum (&c);
	return c;
}

// create a command with one 2-byte integer field. 
command_t cmd_inits (char op, unsigned short id, unsigned short x) {
	command_t c = cmd_init (op, id);
	c.bytes[3] = x >> 8;
	c.bytes[4] = x;
	checksum (&c);
	return c;
}

// create a command with two 2-byte integer fields.
command_t cmd_init2s (char op, unsigned short id, unsigned short x, unsigned short y) {
	command_t c = cmd_init (op, id);
	c.bytes[3] = x >> 8;
	c.bytes[4] = x;
	c.bytes[5] = y >> 8;
	c.bytes[6] = y;
	checksum (&c);
	return c;
}

command_t cmd_initf (char op, unsigned short id, float a) {
	command_t c = cmd_init (op, id);
	getfloat (&c.bytes[3], a);
	checksum (&c);
	return c;
}

command_t cmd_init3fb (char op, unsigned short id, float a, float b, float c, char d) {
	command_t x = cmd_init (op, id);
	getfloat (&x.bytes[3], a);
	getfloat (&x.bytes[7], b);
	getfloat (&x.bytes[11], c);
	x.bytes[15] = d;
	checksum (&x);
	return x;
}

command_t cmd_init4f (char op, unsigned short id, float x, float y, float z, float f) {
	command_t c = cmd_init (op, id);
	getfloat (&c.bytes[3], x);
	getfloat (&c.bytes[7], y);
	getfloat (&c.bytes[11], z);
	getfloat (&c.bytes[15], f);
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

#define N_OPS 33
char *ops[] = {"noop", "mova", "movr", "marc", "mhlx", "home", "clwo", "swox", "swoy", "crot", "srot", "edgx", "edgy", "efmx", "efmy", "ef2x", "ef2y", "stpe", "stpd", "spne", "spnd", "ssps", "wait", "wusr", "beep", "qpos", "qabs", "qwor", "qrot", "qend", "qsps", "echo", "stop"};

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
	unsigned short id = 0;
	do {
		line++;
		id = cmd.size();
		lbp = strchr (s, '\n');	// find the next newline
		if (lbp == NULL) lbp = end;	// if not found, set the end of the current line to the end of the input string

		if (gcode != NULL) {
			gcode->append_line (string (s, (size_t) (lbp-s)));
		}

		s += strspn (s, " \t");	// skip leading whitespace
		// first 4 characters of the line are always the opcode
		int opcode = -1;
		for (int i=0; i < N_OPS; i++) {
			if (strncasecmp (s, ops[i], 4) == 0) {
				opcode = i;
				break;
			}
		}
		if (opcode == -1) {
			error (line, "Bad opcode!");
		} else if (opcode == N_OPS - 1) {
			opcode = 255;	// STOP
		}
		int homeaxes = 0;
		switch (opcode) {
			case MOVA:
			case MOVR:
			case MARC:
			case MHLX:
				cmd.push_back (cmd_init4f (opcode, id++, strtof (s, &s), strtof(s, &s), strtof(s, &s), strtof(s, &s)));
				break;
			case HOME:
				homeaxes = 0;
				while (s < lbp) {
					if (s[0] == 'x') {
						homeaxes |= 1;
					} else if (s[0] == 'y') {
						homeaxes |= 2;
					} else if (s[0] == 'z') {
						homeaxes |= 4;
					} else {
						break;
					}
				}
				cmd.push_back (cmd_initb (opcode, id++, (unsigned char) homeaxes));
				break;
			case SWOX:
			case SWOY:
			case SROT:
			case EDGX:
			case EDGY:
				cmd.push_back (cmd_initf (opcode, id++, strtof (s, &s)));
				break;
			case EFMX:
			case EFMY:
			case EF2X:
			case EF2Y:
				cmd.push_back (cmd_init3fb (opcode, id++, strtof (s, &s), strtof(s, &s), strtof(s, &s), (char) strtol (s, &s, 10)));
				break;
			case NOOP:
			case CLWO:
			case CROT:
			case STPE:
			case STPD:
			case SPNE:
			case SPND:
			case QPOS:
			case QABS:
			case QWOR:
			case QROT:
			case QEND:
			case QSPS:
			case STOP:
				cmd.push_back (cmd_init (opcode, id++));
				break;
			case SSPS:
			case WAIT:
				cmd.push_back (cmd_inits (opcode, id++, (unsigned short) strtol (s, &s, 10)));
				break;
			case WUSR:
				cmd.push_back (cmd_initb (opcode, id++, (unsigned char) strtol (s, &s, 10)));
				break;
			case BEEP:
				cmd.push_back (cmd_init2s (opcode, id++, (unsigned short) strtol (s, &s, 10), (unsigned short) strtol (s, &s, 10)));
				break;
			case ECHO:
				// do something
				break;
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
