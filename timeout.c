#include <stdlib.h>
#include <sys/time.h>
#include "timeout.h"

timeout_t *timeoutAlloc() {
	return (timeout_t *) malloc(sizeof(timeout_t));
}

void timeoutFree(timeout_t *timeout) {
	free(timeout);
}

void timeoutInit(timeout_t *timeout, long millis) {
	gettimeofday(&timeout->tv, NULL);
	timeout->millis = millis;
}

int timeoutIsTimedOut(timeout_t *timeout) {
	struct timeval now;
	gettimeofday(&now, NULL);
	return timeout->millis * 1000LL < ((now.tv_sec - timeout->tv.tv_sec) * 1000000LL + (now.tv_usec - timeout->tv.tv_usec));
}

void timeoutMakeTimeval(timeout_t *timeout, struct timeval *tv) {
	struct timeval now;
	gettimeofday(&now, NULL);

	long long diff = (timeout->millis * 1000LL) - ((now.tv_sec - timeout->tv.tv_sec) * 1000000LL + (now.tv_usec - timeout->tv.tv_usec));

	if (diff < 0) {
		tv->tv_sec = 0;
		tv->tv_usec = 0;
	} else {
		tv->tv_sec = diff / 1000000LL;
		tv->tv_usec = diff % 1000000LL;
	}
}
