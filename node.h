#ifndef NODE_H
#define NODE_H

#include <netdb.h>

#define NODE_ID_MIN 0
#define NODE_ID_MAX 255

typedef struct node_s {
	int id;
	struct sockaddr_in address;
	unsigned int linked : 1;
	unsigned int reachable : 1;
	int distance;
	int serverId;
	struct node_s *next;
} node_t;

/**
 * Serializes node address to A.B.C.D:P string
 * 
 * Function is not thread-safe
 */
char *nodeAddressPretty(node_t *node);

int nodeIdIsValid(int nodeId);

#endif
