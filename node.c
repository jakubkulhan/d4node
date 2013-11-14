#include <stdio.h>
#include "node.h"

static char nodeAddressPrettyBuf[128];

char *nodeAddressPretty(node_t *node) {
	sprintf(
		nodeAddressPrettyBuf,
		"%hhu.%hhu.%hhu.%hhu:%d",
		((char *) &node->address.sin_addr)[0],
		((char *) &node->address.sin_addr)[1],
		((char *) &node->address.sin_addr)[2],
		((char *) &node->address.sin_addr)[3],
		ntohs(node->address.sin_port)
	);

	return nodeAddressPrettyBuf;
}

int nodeIdIsValid(int nodeId) {
	return nodeId >= NODE_ID_MIN && nodeId <= NODE_ID_MAX;
}
