#include "iocore.h"
#include "host.h"
using namespace std;

static int spfd = -1;	// file descriptor number for the serial port 
static vector<command_t> cmd;	// list of commands to send to the router when iocore_run_auto is called
static deque<command_t> sent;	// a list of recently sent commands. This is useful for interpreting and
								// formatting the responses received.
static deque<command_t> manual_cmd;	// manual mode commands to be sent

static uchar response_buffer[256];	// a place to store responses from the router (for commands like get position)
static int response_len = 0;		// number of bytes in the response
static int response_id = 0;			// id of the command to which this is a response. 

static bool running = false;		// whether or not we're running (in auto mode)
int connection = DISCONNECTED;		// status of the connection
static int pos = 0;					// current position in the list of auto commands

pthread_mutex_t iomutex;
pthread_t iothread;		// the IO runs in a separate thread from the GUI to allow responsivity in both.

void iocore_init () {
	pthread_mutex_init (&iomutex, NULL);
	pthread_create (&iothread, NULL, iocore_mainloop, NULL);
}

// this will be called (presumably) from the GUI thread; the modified value 
// will be noticed in iocore_mainloop and the connecting procedure will be started.
void iocore_connect () {
	if (connection == DISCONNECTED) {
		connection = PENDING;
	}
}

// disconnect from the router. Won't do it if we're currently running.
void iocore_disconnect () {
	if (connection == CONNECTED) {
		if (!running) {
			connection = DISCONNECTED;
			serialport_close (spfd) == 0;
		} else {
			console_append ("Can't disconnect while running.");
		}
	} else {
		console_append ("Already disconnected.");
	}
}

// load a new job.
void iocore_load (vector<command_t> c) {
	if (!running) {
		cmd = c;
		pos = 0;
	}
}

// run the currently loaded job. Check to verify that we're connected, not
// running, and there is a loaded job.
void iocore_run_auto () {
	if (connection != CONNECTED) {
		console_append ("Must connect to router first.");
		return;
	}
	if (cmd.empty()) {
		console_append ("No commands loaded.");
		return;
	}
	if (!running) {
		pos = 0;
		running = true;
	}
}

/* The iocore_run_manual methods are used to run just a single command
 * (for iocore_run_manual) or a list of commands (iocore_run_manualv), while
 * keeping a job loaded. The intent is that these are used when the user presses
 * a button on the host GUI to move/home the axes, or enters a single line of gcode
 * in the textfield */
bool iocore_run_manualv (vector<command_t> c) {
	if (running) {
		console_append ("Can't run manual commands while job is running");
		return false;
	}
	if (connection == CONNECTED) {
	//	pthread_mutex_lock (&iomutex);
		for (int i=0; i < c.size(); i++) {
			manual_cmd.push_back (c[i]);
		}
	//	pthread_mutex_unlock (&iomutex);
		return true;
	} else {
		console_append ("Must connect to the router first!");
	}
	return false;
}

bool iocore_run_manual (command_t c) {
	printf ("iocore was asked to run the command: \n");
	cmd_println (c);
	if (running) {
		console_append ("Can't run manual commands while job is running");
		return false;
	}
	if (connection == CONNECTED) {
		pthread_mutex_lock (&iomutex);
		manual_cmd.push_back (c);
		pthread_mutex_unlock (&iomutex);
		return true;
	} else {
		console_append ("Must connect to the router first!");
	}
	return false;
}

/* This takes care of actually sending a command to the router. */ 
void send_command (command_t c) {
	if (CONSOLE_SEND) console_append (string ("Sending command: " + cmd_getstring (c)));

	serialport_write (spfd, &c.bytes, COM_SIZE);
	sent.push_front (c);
	if (sent.size() > BUFFER_SIZE) {
		sent.pop_back ();
	}
}

/* Here's the main I/O loop. First we check our connection state. If it is disconnected,
 * we wait for 1/4 second and go around again. If someone has set our state to PENDING
 * by calling iocore_connect, then we'll attempt to open the serial port, and do the 
 * opening handshake. If we're connected, we'll go and try to read from the serial port
 * and do the appropriate thing with what we get.
*/

/* The actual communications protocol is implemented by a simple finite state machine.
 * See the 'protocol' file for more info on how this works */

static int state = IDLE;
static int substate = 0;

void *iocore_mainloop (void *arg) {	// the odd paramter profile is mandated by pthread
	char inp;
	int n;	// will store how many bytes off the serial port are waiting to be processed

	while (true) {
		if (connection == DISCONNECTED) {	// if we're not connected, check back in 1/4 second.
			usleep (1000 * 250);
			continue;
		}
		if (connection == PENDING) {		// someone has told us to connect.
			spfd = serialport_init (SERIAL_PORT_NAME, BAUDRATE);
			if (spfd == -1) {
				console_append ("Couldn't open serial port.");
				connection = DISCONNECTED;
				connect.setText ("Connect");
				continue;
			}
			sleep (1);	// give the Arduino a little time to wake up

			while (serialport_read (spfd, &inp) <= 0) {	// while the read either fails or there's no data coming in
				// every second, send a byte with value 1 to the router until it responds to us.
				char x = 1;
				if (CONSOLE_PING) console_append (string ("ping"));
				serialport_write (spfd, &x, 1);
				sleep (1);
			}
			n = 1;
			connect.setText ("Disconnect");
			connection = CONNECTED;

			printf ("Connected\n");
		}
		if (!running && !manual_cmd.empty()) {	// if there are manual commands to run and we're not running a job, run the first one
			send_command (manual_cmd[0]);
			manual_cmd.pop_front();
		}
		// if we're in auto mode and we haven't run a command yet, get the first one going. This needs to be 
		// special-cased because in general commands are sent in response to an acknowledgement that the previous
		// command has been received by the router. You have to knock over the first domino.
		if (running && pos == 0) {
			printf ("Sending first command in auto mode\n");
			send_command (cmd[0]);
			pos++;
		}


		if (n > 0) {	// if we have serial data to process (the actual read takes place at the end of this loop)
			printf ("Recv: %c (%d)\n", inp, inp);
			
			if (state == IDLE) {
				switch (inp) {
					case 'a':
						state = ACK;	substate = 1;	break;
					case 't':
						state = RETRANSMIT;	substate = 1;	break;
					case 'r':
						state = RESPONSE; substate = 1; break;
					case 'A':
						state = ABORT;	
						console_append ("Endstop or maximum coordinate hit during move! Press resume button");	break;
					default:
						console_append ("Unexpected byte received!");
						printf ("Unexpected byte received: %c (%d)\n", inp, inp);
				}
			} else if (state == ACK) {
				// if we're here, it means inp is the ID number of the command that got ACKed, and so the ACK is complete.
				if (CONSOLE_ACK) {
					char s[10];
					sprintf (s, "ACK %d", inp);
					console_append (string (s));
				}
				state = IDLE;
				substate = 0;
				// the ACK is our cue to send the next command
				if (running) {	
				   	if (pos < cmd.size()) {
						send_command (cmd[pos++]);
					} else {
						printf ("COmmands exhaiusted. Running = false\n");
						running = false;
					}
				} else {
					if (!manual_cmd.empty()) {
						send_command (manual_cmd[0]);
						manual_cmd.pop_front();
					}
				}

			} else if (state == RESPONSE) {
				/* Responses are structured as follows:
				 * byte 0: 'r'
				 * byte 1: ID number of the command to which this is a response
				 * byte 2: number of bytes in the data for the response
				 * [data]
				 *
				 * The 'substate' variable is used to keep track of where we are.
				*/
				if (substate == 1) {
					response_id = inp;
					memset (response_buffer, 0, 256);
				} else if (substate == 2) {
					response_len = inp;
				} else {
					response_buffer[substate - 3] = inp;
				}
				substate++;
				if (substate - 3 == response_len) {	// we've received as many bytes as the router told us it wanted to send
					state = IDLE;
					substate = 0;
					print_response();
				}
			} else if (state == RETRANSMIT) {
				if (inp == 'x') {
					retransmit();
				} else {
					console_append ("Expecting 'x' after 't' for retransmit\n");
				}
				state = IDLE;
				substate = 0;
			} else if (state == ABORT) {
				// the only way to get out of the ABORT state is for the router to send a 'C' indicating things are clear
				if (inp == 'C') {
					state = IDLE;
				}
			}
		}
		usleep (1000);	// run through the loop every millisecond
		n = serialport_read (spfd, &inp);	// now make the read on the serial port
	}
}

// resend the last sent command (top of the 'sent' list)
void retransmit () {
	console_append ("Retransmitting command");
	cmd_println (sent[0]);

	serialport_write (spfd, &(sent[0].bytes), 11);
}


/* Here are methods to format the responses from the router according to the command
 * type that provoked them */

// utility method for endianness conversion in the response buffer
uint16_t get16 (int idx) {
	return (((uint16_t) response_buffer[idx*2]) << 8) + response_buffer[idx*2+1];
}

void print_response () {
	// search for the most recently sent command with the ID indicated in the response
	int idx = -1;
	for (int i = 0; i < sent.size(); i++) {
		if (sent[i].bytes[1] == response_id) {
			idx = i;
			break;
		}
	}
	comtype_t ct;
	if (idx == -1) {	// if no command was found, treat it as an ECHO command
		ct = ECHO;
	} else {
		ct = (comtype_t) sent[idx].bytes[0];
	}

	printf ("Response to %d\n", response_id);
	uchar r0 = response_buffer[0];
	char buf[200];
	switch (ct) {
		case QPOS:
			sprintf (buf, "X: %f   Y: %f   Z: %f", get16(0) * 0.01f, get16(1) * 0.01f, get16(2) * 0.01f);
			break;
		case QEND:
			sprintf (buf, "Endstops: X: %d  Y: %d  Z: %d", r0 & 1, (r0 & 2) >> 1, (r0 & 4) >> 2);
			break;
		case QSPS:
			sprintf (buf, "Spindle speed: %d rpm", (int) (SP_SPEED_MIN + (get16(1) / 1024.0f) * (SP_SPEED_MAX - SP_SPEED_MIN)));
			break;
		case ECHO:
		default:
			response_buffer[response_len] = 0;	// ensure we have null-termination
			sprintf (buf, "ECHO: %s", response_buffer);
	}
	console_append (string (buf));
}
