#include "gui.h"

bool Rect::contains (int px, int py) {
	return px >= x && px < x+w && py >= y && py < y+h;
}

// Component functions

void Component::render () {
}
Component* Component::clicked (int b, int s, int x, int y) {
}

// Container functions

void Container::render () {
	glPushMatrix();
	glTranslatef (bounds.x, bounds.y, 0);
	for (int i=0; i < children.size(); i++) {
		children[i]->render();
	}
	glPopMatrix();
}


void Container::add (Component *c) {
	children.push_back (c);
}

Component * Container::clicked (int button, int state, int x, int y) {
	x -= bounds.x;
	y -= bounds.y;
	for (int i = children.size() - 1; i >= 0; i--) {
		if (children[i]->bounds.contains (x, y)) {
			return children[i]->clicked (button, state, x, y);
		}
	}
	return NULL;
}

// Button functions

void Button::setText (string str) {
	text = str;
	glutPostRedisplay();
}

void Button::render () {
	if (pressed) {
		glColor3f (1,0,0);
	} else {
		glColor3f (1,1,1);
	}
	glLineWidth (2);
	glBegin (GL_LINE_LOOP);
	glVertex2i (bounds.x, bounds.y);
	glVertex2i (bounds.x, bounds.y + bounds.h);
	glVertex2i (bounds.x + bounds.w, bounds.y + bounds.h);
	glVertex2i (bounds.x + bounds.w, bounds.y);
	glEnd();
	int len = text.length();
	glRasterPos3i (bounds.x + bounds.w/2 - (len * 4), bounds.y + bounds.h/2 + 5, 0);
	for (int i=0; i < len; i++) {
		glutBitmapCharacter (GLUT_BITMAP_8_BY_13, text[i]);
	}
}

void Button::setCallback (void (*cb) (Component *, int, int)) {
	click_callback = cb;
}

Component* Button::clicked (int button, int state, int x, int y) {
	if (sticky) {
		pressed = !pressed;
	} else {
		pressed = state == GLUT_DOWN;
	}
	glutPostRedisplay();
	if (click_callback != NULL) {
		click_callback (this, button, state);
	}
	return pressed ? this : NULL;
}

/* Buttongroup methods */

void Buttongroup::render () {
	glPushMatrix();
	glTranslatef (bounds.x, bounds.y, 0);
	for (int i=0; i < buttons.size(); i++) {
		if (i != selected) buttons[i]->render();
	}
	if (selected != -1) buttons[selected]->render();
	glPopMatrix();
}

Component* Buttongroup::clicked (int b, int s, int x, int y) {
	x -= bounds.x;
	y -= bounds.y;
	for (int i=0; i < buttons.size(); i++) {
		if (buttons[i]->bounds.contains (x, y)) {
			if (selected != -1) {
				buttons[selected]->pressed = false;
			}
			selected = i;
			buttons[i]->pressed = true;
		}
	}
	glutPostRedisplay();
	printf ("selected = %d\n", selected);
	return NULL;	// can't focus a buttongroup
}


/* TEXTFIELD methods */

void Textfield::render () {
	glLineWidth (1);
	if (focus == this) {
		glColor3f (0.7f, 1.0f, 0.7f);
	} else {
		glColor3f (1.0f, 1.0f, 1.0f);
	}
	glBegin (GL_LINE_LOOP);
	glVertex2i (bounds.x, bounds.y);
	glVertex2i (bounds.x, bounds.y + bounds.h);
	glVertex2i (bounds.x + bounds.w, bounds.y + bounds.h);
	glVertex2i (bounds.x + bounds.w, bounds.y);
	glEnd();

	glRasterPos3i (bounds.x + 4, bounds.y + bounds.h/2 + 5, 0);
	for (int i=0; i < text.length(); i++) {
		glutBitmapCharacter (GLUT_BITMAP_8_BY_13, text[i]);
		if ((i * 8 + 20) > bounds.w) return;
	}
	if (focus == this) {
		glutBitmapCharacter (GLUT_BITMAP_8_BY_13, '_');
	}
}

Component* Textfield::clicked (int b, int s, int x, int y) {
	glutPostRedisplay();
	return this;
}

void Textfield::clear () {
	text.clear();
	glutPostRedisplay();
}

void Textfield::setText (string s) {
	text = s;
	glutPostRedisplay();
}

string Textfield::getText () {
	return text;
}

void Textfield::keypress (unsigned char c){
	printf ("Textfield got character %c (%d)\n", c, c);
	if (c == 8) {	// backspace
		if (text.length() >= 1) {
			text.erase (text.length() - 1);
		}
	} else if (c == '\r') {	// enter -> invoke the callback
		if (enter_callback) {
			enter_callback (this);
		}
	} else {
		if (isprint(c)) {
			text.push_back (c);
		}
	}
	glutPostRedisplay();
}

/* Label methods */

void Label::render () {
	glColor3f (1,1,1);
	if (border) {
		glLineWidth (1);
		glBegin (GL_LINE_LOOP);
		glVertex2i (bounds.x, bounds.y);
		glVertex2i (bounds.x, bounds.y + bounds.h);
		glVertex2i (bounds.x + bounds.w, bounds.y + bounds.h);
		glVertex2i (bounds.x + bounds.w, bounds.y);
		glEnd();
	}

	glRasterPos3i (bounds.x + 4, bounds.y + bounds.h/2 + 5, 0);
	for (int i=0; i < text.length(); i++) {
		glutBitmapCharacter (GLUT_BITMAP_8_BY_13, text[i]);
		if ((i * 8 + 20) > bounds.w) return;
	}
}

void Label::setText (string t) {
	text = t;
	glutPostRedisplay();
}

/* Textscroller methods */
void Textscroller::render () {
	glColor3f (1,1,1);
	// render the border 
	glLineWidth (2);
	glBegin (GL_LINE_LOOP);
	glVertex2i (bounds.x, bounds.y);
	glVertex2i (bounds.x, bounds.y + bounds.h);
	glVertex2i (bounds.x + bounds.w, bounds.y + bounds.h);
	glVertex2i (bounds.x + bounds.w, bounds.y);
	glEnd();

	// render the scrollbar
	if (lines.size() > display_lines) {
		int spos = (int) ((scrollpos / (double) lines.size()) * bounds.h);
		int sht = (int) ((display_lines / (double) lines.size()) * bounds.h);
		if (sht < MIN_SCROLLBAR_HT) sht = MIN_SCROLLBAR_HT;

		int xpos = bounds.x + bounds.w - 10;
		glLineWidth (1);
		glBegin (GL_LINE_LOOP);
		glVertex2i (xpos, bounds.y + spos);
		glVertex2i (xpos, bounds.y + spos + sht);
		glVertex2i (xpos + 7, bounds.y + spos + sht);
		glVertex2i (xpos + 7, bounds.y + spos);
		glEnd();
	}
	
	// render the text
	for (int i = scrollpos; i < min(scrollpos + display_lines, (int) lines.size()); i++) {
		glRasterPos3i (bounds.x + 4, bounds.y + 2 + (i-scrollpos + 1) * TS_LINE_HT, 0);
		for (int j=0; j < lines[i].length(); j++) {
			glutBitmapCharacter (GLUT_BITMAP_8_BY_13, lines[i][j]);
			if ((j*8 + 30) > bounds.w) break;
		}
	}

}


Component *Textscroller::clicked (int s, int b, int x, int y) {
	return this;
}
	

void Textscroller::keypress (unsigned char c) {
	if (c == 'j') {
		scrollpos ++;
	} else if (c == 'k') {
		scrollpos --;
	} else if (c == 'u') {
		scrollpos -= display_lines - 2;
	} else if (c == 'd') {
		scrollpos += display_lines - 2;
	} else {
		return;
	}
	if (scrollpos >= (int) lines.size() - display_lines) scrollpos = lines.size()-display_lines;
	if (scrollpos < 0) scrollpos = 0;
	glutPostRedisplay();
}


void Textscroller::append_line (string str) {
	lines.push_back (str);
	if ((int) lines.size() == max_capacity) {
		lines.pop_front();
	}
//	glutPostRedisplay();
}

void Textscroller::clear () {
	lines.clear();
	glutPostRedisplay();
}
