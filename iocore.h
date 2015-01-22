#ifndef IOCORE_H
#define IOCORE_H

#include "serial.h"
#include "config.h"
#include "command.h"
#include <vector>
#include <deque>

#define IDLE 0
#define ACK 1
#define RESPONSE 2
#define RETRANSMIT 3

#define BUFFER_SIZE 16

bool iocore_init ();
void iocore_load ( std::vector< command_t>);

void iocore_run ();
void print_response();

void retransmit();

#endif
