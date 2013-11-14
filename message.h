#ifndef MESSAGE_H
#define MESSAGE_H

#include "timeout.h"

typedef struct message_s {
	int from;
	int to;
	long id;
	char *text;
	timeout_t timeout;
	int tries;
	struct message_s *next;
} message_t;

message_t *messageAlloc();

void messageFree(message_t *message);

#endif
