#include "iocore.h"
#include "host.h"
using namespace std;

static int spfd = -1;
static vector<command_t> cmd;
static deque<command_t> sent;
static deque<command_t> manual_cmd;

static uchar response_buffer[256];
static int response_len = 0;
static int response_id = 0;

static bool running = false;
int connection = DISCONNECTED;
static int pos = 0;

pthread_mutex_t iomutex;
pthread_t iothread;

void iocore_init () {
	pthread_mutex_init (&iomutex, NULL);
	pthread_create (&iothread, NULL, iocore_mainloop, NULL);
}

void iocore_connect () {
	if (connection == DISCONNECTED) {
		connection = PENDING;
	}
}

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

void iocore_load (vector<command_t> c) {
	if (!running) {
		cmd = c;
		pos = 0;
	}
}

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

void send_command (command_t c) {
	if (CONSOLE_SEND) console_append (string ("Sending command: " + cmd_getstring (c)));

	serialport_write (spfd, &c.bytes, COM_SIZE);
	sent.push_front (c);
	if (sent.size() > BUFFER_SIZE) {
		sent.pop_back ();
	}
}

static int state = IDLE;
static int substate = 0;

void *iocore_mainloop (void *arg) {
	char inp;
	int n;

	while (true) {
		if (connection == DISCONNECTED) {
			usleep (1000 * 250);
			continue;
		}
		if (connection == PENDING) {
			spfd = serialport_init (SERIAL_PORT_NAME, BAUDRATE);
			if (spfd == -1) {
				console_append ("Couldn't open serial port.");
				connection = DISCONNECTED;
				connect.setText ("Connect");
				continue;
			}
			sleep (1);

			while (serialport_read (spfd, &inp) <= 0) {
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
		if (!running && !manual_cmd.empty()) {
			send_command (manual_cmd[0]);
			manual_cmd.pop_front();
		}
		if (running && pos == 0) {	// send first command, since there will be no preceeding ack, in general.
			printf ("Sending first command in auto mode\n");
			send_command (cmd[0]);
			pos++;
		}


		if (n > 0) {
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
				if (CONSOLE_ACK) {
					char s[10];
					sprintf (s, "ACK %d", inp);
					console_append (string (s));
				}
				state = IDLE;
				substate = 0;
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
				if (substate == 1) {
					response_id = inp;
					memset (response_buffer, 0, 256);
				} else if (substate == 2) {
					response_len = inp;
				} else {
					response_buffer[substate - 3] = inp;
				}
				substate++;
				if (substate - 3 == response_len) {
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
				if (inp == 'C') {
					state = IDLE;
				}
			}
		}
		usleep (1000);
		n = serialport_read (spfd, &inp);
	}
}

void retransmit () {
	console_append ("Retransmitting command");
	cmd_println (sent[0]);

	serialport_write (spfd, &(sent[0].bytes), 11);
}

uint16_t get16 (int idx) {
	return (((uint16_t) response_buffer[idx*2]) << 8) + response_buffer[idx*2+1];
}

void print_response () {
	// find it in our buffer
	int idx = -1;
	for (int i = 0; i < sent.size(); i++) {
		if (sent[i].bytes[1] == response_id) {
			idx = i;
			break;
		}
	}
	comtype_t ct;
	if (idx == -1) {
		ct = ECHO;
	} else {
		ct = (comtype_t) sent[idx].bytes[0];
	}

	printf ("Response to %d\n", response_id);
	uchar r0 = response_buffer[0];
	char buf[200];
	switch (ct) {
		case GET_POSITION:
			sprintf (buf, "X: %f   Y: %f   Z: %f", get16(0) * 0.01f, get16(1) * 0.01f, get16(2) * 0.01f);
			break;
		case GET_ENDSTOPS:
			sprintf (buf, "Endstops: X: %d  Y: %d  Z: %d", r0 & 1, (r0 & 2) >> 1, (r0 & 4) >> 2);
			break;
		case GET_SPINDLE_SPEED:
			sprintf (buf, "Spindle speed: %d rpm", (int) (SP_SPEED_MIN + (get16(1) / 1024.0f) * (SP_SPEED_MAX - SP_SPEED_MIN)));
			break;
		case ECHO:
		default:
			response_buffer[response_len] = 0;	// ensure we have null-termination
			sprintf (buf, "ECHO: %s", response_buffer);
	}
	console_append (string (buf));
}
