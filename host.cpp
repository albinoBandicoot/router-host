#include "host.h"
#include <sys/time.h>
#include <errno.h>
#include <iostream>
using namespace std;

pthread_mutex_t redisplay_mut;
pthread_cond_t redisplay_cond;

/* GUI COMPONENTS */
int WIN_XS = DEFAULT_WIN_XS;
int WIN_YS = DEFAULT_WIN_YS;

// button callbacsk
void moveaxis_callback (Component *c, int b, int s);
void homeaxis_callback (Component *c, int b, int s);
void load_callback (Component *c, int b, int s);
void connect_callback (Component *c, int b, int s);
void run_callback (Component *c, int b, int s);
void estop_callback (Component *c, int b, int s);
void gcode_entry_callback (Textfield *tf);

Container root (Rect(0,0,WIN_XS,WIN_YS));

Container xyz (Rect (WIN_XS - 320, WIN_YS - 220, 300, 200));

Container xy (Rect (0, 0, 200, 200));
Button xhome (Rect (60, 80, 80, 20), "Home X", homeaxis_callback);
Button yhome (Rect (60, 100, 80, 20), "Home Y", homeaxis_callback);
Button xup (Rect (150, 60, 50, 80), "+X", moveaxis_callback);
Button xdown (Rect (0, 60, 50, 80), "-X", moveaxis_callback);
Button ydown (Rect (60, 130, 80, 40), "-Y", moveaxis_callback);
Button yup (Rect (60, 30, 80, 40), "+Y", moveaxis_callback);

Container zc (Rect (210, 0, 100, 200));
Button zhome (Rect (10, 80, 80, 40), "Home Z", homeaxis_callback);
Button zup (Rect (10, 30, 80, 40), "+Z", moveaxis_callback);
Button zdown (Rect (10, 130, 80, 40), "-Z", moveaxis_callback);

Button xyzhome (Rect (0,0,0,0), "HOME XYZ", homeaxis_callback);

Button connect (Rect (10, 10, 130, 20), "Connect", connect_callback);
Button load_file (Rect (150, 10, 90, 20), "Load", load_callback);
Button runbutton (Rect (250, 10, 90, 20), "Run", run_callback);
Button estop (Rect (350, 10, 90, 20), "ESTOP", estop_callback);
Textfield file_name (Rect (450, 10, WIN_XS-470, 20), "(filename)");
Container topbar (Rect (0, 0, WIN_XS, 40));

Textfield gcode_input (Rect (80, 460, WIN_XS-90, 25), "", gcode_entry_callback);
Label gcode_input_label (Rect (10, 460, 70, 25), "Gcode:");

Label xpos (Rect (0, 0, 120, 25), "X: 0.00", true);
Label ypos (Rect (0, 30, 120, 25), "Y: 0.00", true);
Label zpos (Rect (0, 60, 120, 25), "Z: 0.00", true);
Container positions (Rect (WIN_XS - 220, 10, 200, 90));

Textscroller console (Rect (10, 500, WIN_XS-20, WIN_YS-510), 128);
Textscroller gcode (Rect (10, 45, WIN_XS - 300, 400), -1);

Buttongroup move_amounts (Rect (WIN_XS- 320, 300, 300, 25));
Button move_001 (Rect(0,0,60,25), "0.01", true);
Button move_01 (Rect(60,0,60,25), "0.1", true);
Button move_1  (Rect(120,0,60,25), "1", true);
Button move_10 (Rect(180,0,60,25), "10", true);
Button move_100 (Rect(240,0,60,25), "100", true);

Buttongroup feedrates (Rect (WIN_XS - 320, 340, 300, 25));
Button feed_slow (Rect(0,0,100,25), SLOW_FEEDRATE_STR, 25);
Button feed_med (Rect(100,0,100,25), MED_FEEDRATE_STR, 25);
Button feed_fast (Rect(200,0,100,25), FAST_FEEDRATE_STR, 25);

Component *focus = NULL;

void setup_gui () {
	xy.add (&xhome);
	xy.add (&yhome);
	xy.add (&xup);
	xy.add (&xdown);
	xy.add (&yup);
	xy.add (&ydown);

	zc.add (&zhome);
	zc.add (&zup);
	zc.add (&zdown);

	xyz.add (&xy);
	xyz.add (&zc);

	positions.add (&xpos);
	positions.add (&ypos);
	positions.add (&zpos);

	topbar.add (&connect);
	topbar.add (&load_file);
	topbar.add (&runbutton);
	topbar.add (&estop);
	topbar.add (&file_name);

	move_amounts.add (&move_001);
	move_amounts.add (&move_01);
	move_amounts.add (&move_1);
	move_amounts.add (&move_10);
	move_amounts.add (&move_100);

	feedrates.add (&feed_slow);
	feedrates.add (&feed_med);
	feedrates.add (&feed_fast);

	root.add (&xyz);
	//root.add (&positions);
	root.add (&topbar);
	root.add (&gcode_input);
	root.add (&gcode_input_label);
	root.add (&console);
	root.add (&gcode);
	root.add (&move_amounts);
	root.add (&feedrates);

}

void do_layout () {
	xyz.setBounds (Rect (WIN_XS - 320, 100, 300, 200));
	console.setBounds (Rect (10, 500, WIN_XS-20, WIN_YS-510));
	gcode.setBounds (Rect (10, 45, WIN_XS-340, 400));
	move_amounts.setBounds (Rect (WIN_XS- 320, 300, 300, 25));
	gcode_input.setBounds (Rect (80, 460, WIN_XS-90, 25));
	topbar.setBounds (Rect (0, 0, WIN_XS, 40));
	feedrates.setBounds (Rect (WIN_XS - 320, 340, 300, 25));
}

void moveaxis_callback (Component *c, int b, int s) {
	if (s == GLUT_UP) return;
	int sel = move_amounts.selected;
	if (sel == -1) {
		console_append ("Please select an amount to move.");
		return;
	}
	int fsel = feedrates.selected;
	if (fsel == -1) {
		console_append ("Please select a feedrate.");
		return;
	}
	float amounts[] = {0.01f, 0.1f, 1, 10, 100};
	float amt = amounts[sel];
	float feed = FEEDRATES[fsel];
	if (c == &xdown) {
		iocore_run_manual (cmd_init4f (RELMOVE, 0, -amt, 0, 0, feed));
	} else if (c == &xup) {
		iocore_run_manual (cmd_init4f (RELMOVE, 0, amt, 0, 0,  feed));
	} else if (c == &ydown) {
		iocore_run_manual (cmd_init4f (RELMOVE, 0, 0, -amt, 0, feed));
	} else if (c == &yup) {
		iocore_run_manual (cmd_init4f (RELMOVE, 0, 0, amt, 0,  feed));
	} else if (c == &zdown) {
		iocore_run_manual (cmd_init4f (RELMOVE, 0, 0, 0, -amt, feed));
	} else if (c == &zup) {
		iocore_run_manual (cmd_init4f (RELMOVE, 0, 0, 0, amt,  feed));
	}
}

void homeaxis_callback (Component *c, int b, int s) {
	if (s == GLUT_UP) return;
	if (c == &xhome) {
		iocore_run_manual (cmd_initb (HOME, 0, 1));
	} else if (c == &yhome) {
		iocore_run_manual (cmd_initb (HOME, 0, 2));
	} else if (c == &zhome) {
		iocore_run_manual (cmd_initb (HOME, 0, 4));
	} else if (c == &xyzhome) {
		iocore_run_manual (cmd_initb (HOME, 0, 7));
	}
}

void gcode_entry_callback (Textfield *tf) {
	console_append (tf->text);
	vector<command_t> cmd = parse_gcode ((char *) (tf->text.c_str()), NULL);
	tf->clear();
	if (err) {
		console_append ("Malformed G-code; not sending.");
	} else {
		iocore_run_manualv (cmd);
	}
}

void load_callback (Component *c, int b, int s) {
	if (s == GLUT_UP) return;

	gcode.clear();
	FILE *gc = fopen (file_name.text.c_str(), "r");
	if (gc == NULL) {
		console_append ("Could not open file.");
		return;
	}
	fseek (gc, 0, SEEK_END);
	long len = ftell (gc);
	rewind (gc);

	char *fstr = (char *) (malloc (len));
	fread (fstr, len, 1, gc);
	fclose (gc);

	vector <command_t> cmd = parse_gcode (fstr, &gcode);

	if (err) {
		console_append ("Malformed GCODE.");
		return;
	}

	for (int i=0; i < cmd.size(); i++) {
		cmd_println (cmd[i]);
	}

	iocore_load (cmd);

}

static const string constrings[3] = {"Connect", "Connecting...", "Disconnect"};

void connect_callback (Component *c, int b, int s) {
	printf ("Connect pressed; status = %d\n", s);
	if (s == GLUT_UP) {
		connect.setText (constrings[connection]);
		return;
	}
	switch (connection) {
		case CONNECTED:
			iocore_disconnect();	break;
		case DISCONNECTED:
			iocore_connect();	break;
	}
	connect.setText (constrings[connection]);
}

void run_callback (Component *c, int b, int s){
	if (s == GLUT_UP) return;
	iocore_run_auto ();
}

void estop_callback (Component *c, int b, int s){
}

void console_append (string str) {
	cout << str << endl;
	console.scrollpos = max (0, (int) (console.lines.size() + 1 - console.display_lines));
	console.append_line (str);
	pthread_cond_signal (&redisplay_cond);
}

void display () {
	glClear (GL_COLOR_BUFFER_BIT);
	glColor3f (1.0f, 1.0f, 1.0f);
	root.render();
	glutSwapBuffers();
}

void mouse (int button, int state, int x, int y) {
	focus = root.clicked (button, state, x, y);
	if (state == GLUT_DOWN && (button == 3 || button == 4)) {	// mouse wheel
		Textscroller *ts = dynamic_cast<Textscroller *>(focus);
		if (ts != NULL) {
			ts->keypress (button == 4 ? 'j' : 'k');
		}
	}
			
	glutPostRedisplay();	// focus change affects rendering
}

void key (unsigned char key, int x, int y) {
	printf ("focus = %p\n", focus);
	if (focus != NULL) {
		focus->keypress (key);
	}
	printf ("Done with key callback\n");
}

void resize (int wd, int ht) {
	WIN_XS = wd;
	WIN_YS = ht;
	glViewport (0, 0, WIN_XS, WIN_YS);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	gluOrtho2D (0, WIN_XS, WIN_YS, 0);
	glMatrixMode (GL_MODELVIEW);

	do_layout();
}

void idle () {
	struct timespec timeout;
	timeval curr_time;
	gettimeofday (&curr_time, NULL);
	timeout.tv_sec = curr_time.tv_sec;
	timeout.tv_nsec = (curr_time.tv_usec + 50000) * 1000;
	int x = pthread_cond_timedwait (&redisplay_cond, &redisplay_mut, &timeout);
	if (x == ETIMEDOUT) {
	} else {
		glutPostRedisplay();
	}
}

int main (int argc, char** argv) {
	setup_gui();
	pthread_cond_init (&redisplay_cond, NULL);
	pthread_mutex_init (&redisplay_mut, NULL);
	iocore_init ();

	glutInit (&argc, argv);
	glutInitWindowPosition (0,0);
	glutInitWindowSize (WIN_XS, WIN_YS);
	glutInitDisplayMode (GLUT_RGBA | GLUT_DOUBLE);

	int win = glutCreateWindow ("Router Host");

	glutDisplayFunc (display);
	glutMouseFunc (mouse);
	glutReshapeFunc (resize);
	glutKeyboardFunc (key);
	glutIdleFunc (idle);

	glViewport (0, 0, WIN_XS, WIN_YS);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	gluOrtho2D (0, WIN_XS, WIN_YS, 0);
	glMatrixMode (GL_MODELVIEW);

	console.append_line ("Welcome to Router Host 1.0 - Piglet Necromancer");

	glutMainLoop();
}
