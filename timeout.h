#ifndef TIMEOUT_H
#define TIMEOUT_H

#include <sys/time.h>

typedef struct timeout_s {
	struct timeval tv;
	long millis;
} timeout_t;

timeout_t *timeoutAlloc();

void timeoutFree(timeout_t *timeout);

void timeoutInit(timeout_t *timeout, long millis);

int timeoutIsTimedOut(timeout_t *timeout);

void timeoutMakeTimeval(timeout_t *timeout, struct timeval *tv);

#endif
