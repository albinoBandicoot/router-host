#ifndef GUI_H
#define GUI_H

#include "config.h"
#include <vector>
#include <deque>
#include <string>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

using namespace std;

/* Class definitions */

class Image {	// this is here solely to get the image of the piglet necromancer to display for the first 2 seconds after the host starts 
	public:
		int xsize, ysize;
		int mode;
		int bpp;	// bytes per pixel
		void *data;

		Image (FILE *);

};

class Rect {
	public:
		int x, y, w, h;

		Rect () : x(0), y(0), w(0), h(0) {}
		Rect (int xp, int yp, int wp, int hp) : x(xp), y(yp), w(wp), h(hp) {}
		bool contains (int, int);
};

// ancestor class of all GUI components
class Component {
	public:
		Component *parent;
		Rect bounds;

		virtual void setBounds (Rect b) {
			bounds = b;
			glutPostRedisplay();	// maybe amalgamate these somewhere nicer? Or just rely on GLUT to do that
		}

		virtual void render ();
		virtual Component* clicked (int button, int state, int x, int y);
		virtual void keypress (unsigned char c) {}
};

// containers offer a means of grouping components together. Child components of a container
// have coordinates relative to the origin of their container. This allows for ease of moving
// around large numbers of grouped components.
class Container : public Component {
	public:
		vector<Component *> children;

		Container (Rect b) {
			bounds = b;
		}

		void add (Component *);
		void render ();
		Component* clicked (int, int, int, int);
		void keypress (unsigned char c) {
			printf ("Container keypress\n");
		}
};

class Button : public Component {
	public:
		bool pressed;
		bool sticky;
		string text;

		void (*click_callback) (Component *c, int button, int state);

		Button (Rect b, string t, bool st = false) {
			bounds = b;
			text = t;
			pressed = false;
			click_callback = NULL;
			sticky = st;
		}

		Button (Rect b, string t, void (*cb) (Component *, int, int)) {
			bounds = b;
			text = t;
			pressed = false;
			click_callback = cb;
		}

		
		void setText (string);
		void setCallback (void (*) (Component *, int, int));
		void render ();
		Component* clicked (int, int, int, int);
		void keypress (unsigned char c) {
			printf ("Button keypress\n");
		}
};

// buttongroups are a set of buttons of which only one can be pressed at a time
class Buttongroup : public Component {
	public:
		vector<Button *> buttons;
		int selected;	// index of selected button

		Buttongroup (Rect b) {
			bounds = b;
			selected = -1;
		}

		void add (Button *b) {
			buttons.push_back (b);
		}

		void render ();
		Component *clicked (int, int, int, int);
		void keypress (unsigned char) {}
};

class Textfield : public Component {
	public:
		bool focused;
		string text;

		void (*enter_callback) (Textfield *c);

		Textfield (Rect b, string t, void (*ec)(Textfield *) = NULL) {
			focused = false;
			text = t;
			bounds = b;
			enter_callback = ec;
		}

		void render ();
		Component* clicked (int, int, int, int);
		void clear ();
		void setText (string);
		string getText ();
		void append (char);
		void keypress (unsigned char);
};

class Label : public Component {
	public:
		string text;
		bool border;

		Label (Rect b, string t, bool bord = false) {
			text = t;
			bounds = b;
			border = bord;
		}

		void render ();
		Component* clicked (int b, int s, int x, int y) {
			return this;
		}
		void setText (string);
		void keypress (unsigned char c) {
			printf ("Label keypress\n");
		}
};

// scrolling text display. Does not support editing.
class Textscroller : public Component {
	public:
		deque<string> lines;
		int scrollpos;	// index of first line to display
		int max_capacity;	// start popping lines off the top when we reach this capacity. Set to -1 to disable popping.
		int display_lines;	// how many lines tall the display is

		Textscroller (Rect b, int maxc) {
			scrollpos = 0;
			max_capacity = maxc;
			display_lines = b.h / TS_LINE_HT;
			bounds = b;
		}

		void setBounds (Rect b) {
			bounds = b;
			display_lines = (b.h - 7) / TS_LINE_HT;
			glutPostRedisplay();
		}

		void append_line (string);
		void clear ();
		void render ();
		Component *clicked (int, int, int, int);
		void keypress (unsigned char);

};

extern Component *focus;	// which component has keyboard focus

#endif
