#include "iocore.h"
using namespace std;

static int spfd = -1;
static vector<command_t> cmd;
static deque<command_t> sent;

static uchar response_buffer[256];
static int response_len = 0;
static int response_id = 0;

static bool running = true;
static int pos = 0;

bool iocore_init () {
	spfd = serialport_init (SERIAL_PORT_NAME, BAUDRATE);
	sleep (1);	// wait for connection to be established.
	return spfd != -1;
}

void iocore_load (vector<command_t> c) {
	cmd = c;
	pos = 0;
	running = true;
}

void send_command (command_t c) {
	printf ("Sending command: ");
	cmd_println (c);

	serialport_write (spfd, &c.bytes, COM_SIZE);
	sent.push_front (c);
	if (sent.size() > BUFFER_SIZE) {
		sent.pop_back ();
	}
}

static int state = IDLE;
static int substate = 0;

void iocore_run () {
/*
	uchar x = 0;
	char inp;
	while (true) {
		printf ("Writing %d\n", x);
		serialport_write (spfd, &x, 1);
		//tcflush (spfd, TCIOFLUSH);
		int n = 1;
		while (n > 0) {
			n = serialport_read (spfd, &inp);
			if (n > 0) {
				printf ("RECV: %d\n", inp);
			}
		}
		sleep (1);
		x++;
	}

}
*/
//	send_command (cmd[0]);
//	pos = 1;
	char inp;
	while (serialport_read (spfd, &inp) <= 0) {
		char x = 1;
		printf ("ping\n");
		serialport_write (spfd, &x, 1);
		sleep (1);
	}
	
	printf ("Recv: %c (%d)\n", inp, inp);
	int n = 1;
	while (true) {
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
					default:
						printf ("Unexpected!\n");
				}
			} else if (state == ACK) {
				printf ("ACK %d\n", inp);
				state = IDLE;
				substate = 0;
				if (running && pos < cmd.size()) {
					send_command (cmd[pos++]);
				}

			} else if (state == RESPONSE) {
				if (substate == 1) {
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
					printf ("Expecting 'x' after 't' for retransmit\n");
				}
				state = IDLE;
				substate = 0;
			}
		}
		usleep (1000);
		n = serialport_read (spfd, &inp);
	}
}

void retransmit () {
	printf ("Retransmitting command: ");
	cmd_println (sent[0]);
	serialport_write (spfd, &(sent[0].bytes), 11);
}

uint16_t get16 (int idx) {
	return ((uint16_t) response_buffer[idx*2]) << 8 + response_buffer[idx*2+1];
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
		ct = (comtype_t) 16;
	} else {
		ct = (comtype_t) sent[idx].bytes[0];
	}

	printf ("Response to %d\n", idx);
	uchar r0 = response_buffer[0];
	switch (ct) {
		case GET_POSITION:
			printf ("X: %f   Y: %f   Z: %f\n", get16(1) * 0.01f, get16(2) * 0.01f, get16(3) * 0.01f);
			break;
		case GET_ENDSTOPS:
			printf ("Endstops: X: %d  Y: %d  Z: %d\n", r0 & 1, (r0 & 2) >> 1, (r0 & 4) >> 2);
			break;
		case GET_SPINDLE_SPEED:
			printf ("Spindle speed: %d rpm\n", (int) (SP_SPEED_MIN + (get16(1) / 1024.0f) * (SP_SPEED_MAX - SP_SPEED_MIN)));
			break;
		case ECHO:
		default:
			for (int i=0; i < response_len; i++) {
				putchar (response_buffer[i]);
			}
			putchar ('\n');
	}
}
