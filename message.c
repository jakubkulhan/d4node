#include <stdlib.h>
#include "message.h"
#include "error.h"

static message_t *freeMessages = NULL;

message_t *messageAlloc() {
	if (!freeMessages) {
		if (!(freeMessages = (message_t *) malloc(sizeof(message_t)))) {

		}
	}

	message_t *message = freeMessages;
	freeMessages = freeMessages->next;
	return message;
}

void messageFree(message_t *message) {
	if (message->text) {
		free(message->text);
	}

	message->next = freeMessages;
	freeMessages = message;
}
