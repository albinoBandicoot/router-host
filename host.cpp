#include "iocore.h"
using namespace std;

int main (int argc, const char* argv[]) {

	FILE *gc = fopen (argv[1], "r");
	fseek (gc, 0, SEEK_END);
	long len = ftell (gc);
	rewind (gc);

	char *fstr = (char *) (malloc (len));
	fread (fstr, len, 1, gc);
	fclose (gc);

	vector <command_t> cmd = parse_gcode (fstr);

	for (int i=0; i < cmd.size(); i++) {
		cmd_println (cmd[i]);
	}

	if (iocore_init ()) {
	} else {
		printf ("Serial initialization failed.\n");
		return 1;
	}
	iocore_load (cmd);
	iocore_run();

}
