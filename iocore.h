#ifndef IOCORE_H
#define IOCORE_H

#include "serial.h"
#include "config.h"
#include "command.h"
#include <pthread.h>
#include <vector>
#include <deque>

#define IDLE 0
#define ACK 1
#define RESPONSE 2
#define RETRANSMIT 3
#define ABORT 4

#define DISCONNECTED 0
#define PENDING 1
#define CONNECTED 2

#define BUFFER_SIZE 16

extern int err;
extern int connection;
extern pthread_mutex_t iomutex;
extern pthread_t iothread;

void iocore_init ();
void iocore_load ( std::vector< command_t>);

void iocore_connect ();
void iocore_disconnect ();

bool iocore_run_manual (command_t);
bool iocore_run_manualv ( std::vector<command_t> );
void iocore_run_auto ();

void *iocore_mainloop (void *);
void print_response();

void retransmit();

#endif
