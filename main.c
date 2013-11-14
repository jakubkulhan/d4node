#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/select.h>
#include <limits.h>

#include "timeout.h"
#include "node.h"
#include "message.h"
#include "error.h"

#define BUF_LENGTH 2048
#define NODES_LENGTH (NODE_ID_MAX + 1)

node_t nodes[NODES_LENGTH];
node_t *servers = NULL;

message_t *messagesToBeSent = NULL;
message_t *messagesReceived = NULL;
long messageLastId = 0;

int main(int argc, char **argv) {

	/*
	 * PROCESS COMMAND LINE ARGUMENTS
	 */
	if (argc < 3 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		printf(
			"Usage: %s [ -h ] <id> <cfg>\n"
			"\n"
			"    -h     show this help\n"
			"    <id>   choose this node ID (%d-%d)\n"
			"    <cfg>  path to config file\n"
			"\n"
			"\n"
			"CONFIGURATION\n"
			"\n"
			"    Every line is one statement, one of the following:\n"
			"\n"
			"        node <id> <port> [ <ip> ]\n"
			"\n"
			"            <id>    ID of node in the network\n"
			"            <port>  port the node binds to\n"
			"            <ip>    IP the node bind to, IP is optional meaning the node is bound to localhost\n"
			"\n"
			"        link <id-client> <id-server>\n"
			"\n"
			"            <id-client>  ID of the client node\n"
			"            <id-server>  ID of the server node\n"
			"\n"
			"        trace_timeout <millis>\n"
			"\n"
			"            <millis>  number of milliseconds after which network is retraced\n"
			"\n"
			"        resend_timeout <millis>\n"
			"\n"
			"            <millis>  number of milliseconds after which message is tried to be sent again\n"
			"\n"
			"        acknowledge_timeout <millis>\n"
			"\n"
			"            <millis>  number of milliseconds after which acknowledge is considered to be delivered\n"
			"\n"
			,
			argv[0], NODE_ID_MIN, NODE_ID_MAX
		);

		return 1;
	}

	char *end;
	int id = (int) strtol(argv[1], &end, 10);
	if (*end != '\0' || id < NODE_ID_MIN || id > NODE_ID_MAX) {
		ERROR("first argument is not valid node ID, use -h to see help");
		return 1;
	}

	char *confFilename = argv[2];
	FILE *conf;
	if (!(conf = fopen(confFilename, "r"))) {
		ERROR("could not open configuration file %s", argv[2]);
		return 1;
	}


	/*
	 * PROCESS CONFIGURATION FILE
	 */
	char bufLine[BUF_LENGTH], nodeHost[BUF_LENGTH];
	int confLine, args, nodeId, nodePort, clientId, serverId, i, confErrors = 0;
	struct hostent *resolvedHost;
	int resendTries = 3;
	long traceTimeoutMillis = 1000, resendTimeoutMillis = 5000, acknowledgeTimeoutMillis = resendTries * resendTimeoutMillis;

	for (i = 0; i < NODES_LENGTH; ++i) {
		memset(&nodes[i], sizeof(nodes[i]), 0);
		nodes[i].id = -1;
		nodes[i].distance = INT_MAX;
		nodes[i].serverId = -1;
	}

	for (confLine = 1; fgets(bufLine, BUF_LENGTH, conf); ++confLine) {

		/* strip comments */
		for (i = 0; bufLine[i] != '\0'; ++i) {
			if (bufLine[i] == '#') {
				bufLine[i] = '\0';
				break;
			}
		}

		/* ltrim */
		for (i = 0; bufLine[i] != '\0' && isspace(bufLine[i]); ++i);
		if (i != 0) {
			memmove(bufLine, &bufLine[i], strlen(&bufLine[i]));
		}

		/* rtrim */
		for (i = strlen(bufLine) - 1; i >= 0 && isspace(bufLine[i]); --i) {
			bufLine[i] = '\0';
		}

		/* skip empty lines */
		if (!bufLine[i]) {
			continue;
		}

		/* process commands */
		if ((args = sscanf(bufLine, "node %d %d %s", &nodeId, &nodePort, nodeHost)) >= 2) {
			if (nodeId < NODE_ID_MIN || nodeId > NODE_ID_MAX) {
				ERROR("node ID out of range @ %s:%d", confFilename, confLine);
				++confErrors;
			}

			nodes[nodeId].id = nodeId;
			nodes[nodeId].address.sin_port = htons(nodePort);

			if (args == 2) {
				nodes[nodeId].address.sin_family = AF_INET;
				nodes[nodeId].address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
				strcpy(nodeHost, "<<localhost>>");

			} else {
				resolvedHost = gethostbyname(nodeHost);
				if (!resolvedHost) {
					ERROR("could not resolve host \"%s\" (used by node %d) @ %s:%d", nodeHost, nodeId, confFilename, confLine);
					++confErrors;
				} else {
					nodes[nodeId].address.sin_family = resolvedHost->h_addrtype;
					memcpy(&nodes[nodeId].address.sin_addr, resolvedHost->h_addr_list[0], resolvedHost->h_length);
				}
			}

			DBG("conf -> node %d %d %s(%s)", nodeId, nodePort, nodeHost, nodeAddressPretty(&nodes[nodeId]));


		} else if ((args = sscanf(bufLine, "link %d %d", &clientId, &serverId)) >= 2) {
			if (clientId < NODE_ID_MIN || nodeId > NODE_ID_MAX) {
				ERROR("client ID out of range @ %s:%d", confFilename, confLine);
				++confErrors;
			}

			if (serverId < NODE_ID_MIN || serverId > NODE_ID_MAX) {
				ERROR("server ID out of range @ %s:%d", confFilename, confLine);
				++confErrors;
			}

			if (clientId == id) {
				nodes[serverId].linked = 1;
			}

			DBG("conf -> link %d %d", clientId, serverId);

		} else if ((args = sscanf(bufLine, "trace_timeout %ld", &traceTimeoutMillis)) >= 1) {
			if (traceTimeoutMillis <= 0) {
				ERROR("trace timeout must be > 0 @ %s:%d", confFilename, confLine);
				++confErrors;
			}

		} else if ((args = sscanf(bufLine, "resend_timeout %ld", &resendTimeoutMillis)) >= 1) {
			if (resendTimeoutMillis <= 0) {
				ERROR("resend timeout must be > 0 @ %s:%d", confFilename, confLine);
				++confErrors;
			}

		} else if ((args = sscanf(bufLine, "resend_tries %d", &resendTries)) >= 1) {
			if (resendTries <= 0) {
				ERROR("resend tries must be > 0 @ %s:%d", confFilename, confLine);
				++confErrors;
			}

		} else if ((args = sscanf(bufLine, "acknowledge_timeout %ld", &acknowledgeTimeoutMillis)) >= 1) {
			if (acknowledgeTimeoutMillis <= 0) {
				ERROR("acknowledge timeout must be > 0 @ %s:%d", confFilename, confLine);
				++confErrors;
			}

		} else {
			ERROR("could not process configuration directive \"%s\" @ %s:%d", bufLine, confFilename, confLine);
			++confErrors;
		}
	}

	fclose(conf);

	if (confErrors) {
		FATAL("configuration file %s contains errors", confFilename);
		return 1;
	}


	/*
	 * INITIALIZE SOCKETS
	 */
	for (i = 0; i < NODES_LENGTH; ++i) {
		if (nodes[i].id != -1) {
			if (nodes[i].linked) {
				nodes[i].next = servers;
				servers = &nodes[i];
			}
		}
	}

	if (nodes[id].id == -1) {
		FATAL("node %d not declared in %s", id, confFilename);
		return 1;
	}

	node_t *me = &nodes[id];

	int fd = socket(me->address.sin_family, SOCK_DGRAM, 0);
	if (fd < 0) {
		FATAL("socket() failed");
		return 1;
	}

	if (bind(fd, (struct sockaddr *) &me->address, sizeof(me->address)) < 0) {
		FATAL("bind() failed");
		return 1;
	}


	/*
	 * EVENT LOOP
	 */
	struct timeval tv;
	node_t *server;

	timeout_t *traceTimeout = timeoutAlloc();
	timeoutInit(traceTimeout, 0);

	fd_set readfds;

	for (;;) {
		DBG("eventloop begin");

		/*
		 * DEBUG: DISPLAY CURRENT ACTIVE NODES
		 */ 
		for (i = 0; i < NODES_LENGTH; ++i) {
			if (nodes[i].id != -1) {
				DBG("node %d => id=%d, active=%d, address=%s, linked=%d, reachable=%d, serverId=%d, distance=%d", i, nodes[i].id, nodes[i].id != -1, nodeAddressPretty(&nodes[i]), nodes[i].linked, nodes[i].reachable, nodes[i].serverId, nodes[i].distance);
			}
		}

		/*
		 * RETRACE NETWORK
		 */
		if (timeoutIsTimedOut(traceTimeout)) {
			for (i = 0; i < NODES_LENGTH; ++i) {
				nodes[i].reachable = 0;
				nodes[i].distance = INT_MAX;
				nodes[i].serverId = -1;
			}

			for (server = servers; server; server = server->next) {
				char buf[BUF_LENGTH];
				sprintf(buf, "trace %d", id);
				if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *) &server->address, sizeof(server->address)) < 0) {
					ERROR("sendto() failed @ %s:%d", __FILE__, __LINE__);
				} else {
					DBG("to='%s', req='%s'", nodeAddressPretty(server), buf);
				}
			}

			timeoutInit(traceTimeout, traceTimeoutMillis);
		}

		/*
		 * PROCESS REQUESTS
		 */
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		FD_SET(fd, &readfds);
		timeoutMakeTimeval(traceTimeout, &tv);

		int result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

		if (result < 0) {
			ERROR("select() failed @ %s:%d", __FILE__, __LINE__);
		}

		if (feof(stdin)) {
			break;
		}

		if (result > 0) {
			char bufReceived[BUF_LENGTH];
			int receivedLength;
			memset(bufReceived, 0, BUF_LENGTH);

			if (FD_ISSET(0, &readfds)) {
				if (fgets(bufReceived, BUF_LENGTH, stdin)) {
					char bufMessage[BUF_LENGTH];

					if ((args = sscanf(bufReceived, "%d:%[^\n]", &nodeId, bufMessage)) < 2) {
						WARN("you have to enter messages in \"<id>:<message>\" format");

					} else if (nodeId < NODE_ID_MIN || nodeId > NODE_ID_MAX) {
						WARN("node ID out of range (must be %d-%d, was %d)", NODE_ID_MIN, NODE_ID_MAX, nodeId);

					} else if (nodes[nodeId].id == -1 || !nodes[nodeId].reachable || nodes[nodeId].serverId < 0) {
						WARN("node %d unreachable, message cannot be delivered", nodeId);

					} else {
						message_t *message = messageAlloc();
						message->from = me->id;
						message->to = nodeId;
						message->id = ++messageLastId;
						if (!(message->text = malloc((strlen(bufMessage) + 1) * sizeof(char)))) {
							FATAL("malloc() failed");
							return 1;
						}
						strcpy(message->text, bufMessage);
						timeoutInit(&message->timeout, resendTimeoutMillis);
						message->tries = resendTries;

						char bufSend[BUF_LENGTH];
						sprintf(bufSend, "message %d %d %ld %s", message->to, message->from, message->id, message->text);

						if (sendto(fd, bufSend, strlen(bufSend), 0, (struct sockaddr *) &nodes[nodes[message->to].serverId].address, sizeof(nodes[nodes[message->to].serverId].address)) < 0) {
							ERROR("sendto() failed @ %s:%d", __FILE__, __LINE__);
						} else {
							message->next = messagesToBeSent;
							messagesToBeSent = message;
							INFO("MSG-ID=%ld", message->id);
							DBG("to='%s', req='%s'", nodeAddressPretty(&nodes[nodes[message->to].serverId]), bufSend);
						}
					}
				}
			}

			if (FD_ISSET(fd, &readfds)) {
				node_t remoteNode;
				socklen_t remoteAddressLength = sizeof(remoteNode.address);

				if ((receivedLength = recvfrom(fd, bufReceived, BUF_LENGTH, 0, (struct sockaddr *) &remoteNode.address, &remoteAddressLength)) < 0) {
					ERROR("recvfrom() failed");

				} else {

					DBG("from='%s', req='%s'", nodeAddressPretty(&remoteNode), bufReceived);

					int toId, fromId, valid = 1;
					long messageId;
					char bufRest[BUF_LENGTH];
					memset(bufRest, 0, BUF_LENGTH);

					if ((args = sscanf(bufReceived, "trace %[^\n]", bufRest)) >= 1) {
						DBG("parsed: type='trace', path='%s'", bufRest);

						char *path = bufRest, *pathRest;
						int nodeId = strtol(path, &pathRest, 10), distance, serverId;

						if (!nodeIdIsValid(nodeId)) {
							ERROR("invalid node ID=%d @ %s:%d", nodeId, __FILE__, __LINE__);

						} else {
							if (nodeId == me->id) {
								for (path = pathRest, serverId = -1, distance = 1; *path == ','; path = pathRest, ++distance) {
									path++;
									nodeId = strtol(path, &pathRest, 10);

									if (!nodeIdIsValid(nodeId)) {
										ERROR("invalid node ID=%d @ %s:%d", nodeId, __FILE__, __LINE__);
										valid = 0;
										break;
									}

									if (serverId < 0) {
										serverId = nodeId;
									}

									nodes[nodeId].id = nodeId;
									nodes[nodeId].reachable = 1;

									if (distance < nodes[nodeId].distance) {
										nodes[nodeId].distance = distance;
										nodes[nodeId].serverId = serverId;
									}
								}

								if (valid) {
									me->reachable = 1;
									if (distance < me->distance) {
										me->distance = distance;
										me->serverId = serverId;
									}
								}

							} else {
								for (path = pathRest, serverId = -1, distance = 1; *path == ','; path = pathRest) {
									path++;
									nodeId = strtol(path, &pathRest, 10);

									if (!nodeIdIsValid(nodeId)) {
										ERROR("invalid node ID=%d @ %s:%d", nodeId, __FILE__, __LINE__);
										valid = 0;
										break;
									}

									if (nodeId == me->id) {
										break;
									}
								}

								if (valid) {
									nodeId = -1;
									if (*pathRest == ',') {
										nodeId = strtol(++pathRest, NULL, 10);

										if (!nodeIdIsValid(nodeId)) {
											ERROR("invalid node ID=%d @ %s:%d", nodeId, __FILE__, __LINE__);
											valid = 0;
										}
									}
								}

								if (valid) {
									for (server = servers; server; server = server->next) {
										if (nodeId != -1 && server->id == nodeId) {
											continue;
										}

										char bufSend[BUF_LENGTH];
										sprintf(bufSend, "%s,%d", bufReceived, me->id);

										if (sendto(fd, bufSend, strlen(bufSend), 0, (struct sockaddr *) &server->address, sizeof(server->address)) < 0) {
											ERROR("sendto() failed @ %s:%d", __FILE__, __LINE__);
										} else {
											DBG("to='%s', req='%s'", nodeAddressPretty(server), bufSend);
										}
									}
								}
							}
						}

					} else if ((args = sscanf(bufReceived, "message %d %d %ld %[^\n]", &toId, &fromId, &messageId, bufRest)) >= 4) {
						DBG("parsed: type='message', to=%d, from=%d, id=%ld, message='%s'", toId, fromId, messageId, bufRest);

						if (!nodeIdIsValid(toId)) {
							ERROR("invalid node ID=%d @ %s:%d", toId, __FILE__, __LINE__);
							valid = 0;
						}

						if (!nodeIdIsValid(fromId)) {
							ERROR("invalid node ID=%d @ %s:%d", fromId, __FILE__, __LINE__);
							valid = 0;
						}

						if (valid) {
							if (toId == me->id) {
								message_t *message = NULL, **head;

								for (head = &messagesReceived; *head; ) {
									if (timeoutIsTimedOut(&(*head)->timeout)) {
										message_t *messageToBeFreed = *head;
										*head = (*head)->next;
										messageFree(messageToBeFreed);

									} else {
										if ((*head)->id == messageId) {
											message = *head;
											break;
										} else {
											head = &(*head)->next;
										}
									}
								}

								if (!message) {
									printf("%05d: %s\n", fromId, bufRest);

									message = messageAlloc();
									message->from = fromId;
									message->to = toId;
									message->id = messageId;
									message->text = NULL;
									timeoutInit(&message->timeout, acknowledgeTimeoutMillis);
									message->next = messagesReceived;
									messagesReceived = message;
								}

								char bufSend[BUF_LENGTH];
								sprintf(bufSend, "acknowledge %d %d %ld", message->from, message->to, message->id);

								if (sendto(fd, bufSend, strlen(bufSend), 0, (struct sockaddr *) &nodes[nodes[message->from].serverId].address, sizeof(nodes[nodes[message->from].serverId].address)) < 0) {
									ERROR("sendto() failed @ %s:%d", __FILE__, __LINE__);
								} else {
									DBG("to='%s', req='%s'", nodeAddressPretty(&nodes[nodes[message->from].serverId]), bufSend);
								}

							} else {
								// try to deliver only to active, reachable nodes with server
								// don't bother queueing messages, they will get resent by sender
								if (nodes[toId].id != -1 && nodes[toId].reachable && nodes[toId].serverId >= 0) {
									if (sendto(fd, bufReceived, strlen(bufReceived), 0, (struct sockaddr *) &nodes[nodes[toId].serverId].address, sizeof(nodes[nodes[toId].serverId].address)) < 0) {
										ERROR("sendto() failed @ %s:%d", __FILE__, __LINE__);
									} else {
										DBG("to='%s', req='%s'", nodeAddressPretty(&nodes[nodes[toId].serverId]), bufReceived);
									}
								}
							}
						}

					} else if ((args = sscanf(bufReceived, "acknowledge %d %d %ld", &toId, &fromId, &messageId)) >= 3) {
						DBG("parsed: type='acknowledge', to=%d, from=%d, id=%ld", toId, fromId, messageId);

						if (!nodeIdIsValid(toId)) {
							ERROR("invalid node ID=%d @ %s:%d", toId, __FILE__, __LINE__);
							valid = 0;
						}

						if (!nodeIdIsValid(fromId)) {
							ERROR("invalid node ID=%d @ %s:%d", fromId, __FILE__, __LINE__);
							valid = 0;
						}

						if (valid) {
							if (toId == me->id) {
								message_t **head;

								for (head = &messagesToBeSent; *head; ) {
									if ((*head)->id == messageId) {
										INFO("OK MSG-ID=%ld", messageId);

										message_t *messageToBeFreed = *head;
										*head = (*head)->next;
										messageFree(messageToBeFreed);
										break;

									} else {
										head = &(*head)->next;
									}
								}

							} else {
								// try to deliver only to active, reachable nodes with server
								// don't bother queueing acks, they will get resent by receiver
								if (nodes[toId].id != -1 && nodes[toId].reachable && nodes[toId].serverId >= 0) {
									if (sendto(fd, bufReceived, strlen(bufReceived), 0, (struct sockaddr *) &nodes[nodes[toId].serverId].address, sizeof(nodes[nodes[toId].serverId].address)) < 0) {
										ERROR("sendto() failed @ %s:%d", __FILE__, __LINE__);
									} else {
										DBG("to='%s', req='%s'", nodeAddressPretty(&nodes[nodes[toId].serverId]), bufReceived);
									}
								}
							}
						}

					} else {
						ERROR("received unknown message '%s'", bufReceived);
					}
				}
			}
		}

		/*
		 * RESEND MESSAGES
		 */
		message_t **messageToBeSent;
		for (messageToBeSent = &messagesToBeSent; *messageToBeSent; ) {
			if ((*messageToBeSent)->tries < 1) {
				INFO("FAILED MSG-ID=%ld", (*messageToBeSent)->id);

				message_t *messageToBeFreed = *messageToBeSent;
				*messageToBeSent = (*messageToBeSent)->next;
				messageFree(messageToBeFreed);

			} else {
				if (timeoutIsTimedOut(&(*messageToBeSent)->timeout)) {
					(*messageToBeSent)->tries--;
					timeoutInit(&(*messageToBeSent)->timeout, resendTimeoutMillis);

					if (nodes[(*messageToBeSent)->to].id < 0 || !nodes[(*messageToBeSent)->to].reachable || nodes[(*messageToBeSent)->to].serverId < 0) {
						WARN("resending MSG-ID=%ld failed, node %d still unreachable (%d tries remaining)\n", (*messageToBeSent)->id, (*messageToBeSent)->to, (*messageToBeSent)->tries);

					} else {
						char bufSend[BUF_LENGTH];
						sprintf(bufSend, "message %d %d %ld %s", (*messageToBeSent)->to, (*messageToBeSent)->from, (*messageToBeSent)->id, (*messageToBeSent)->text);

						if (sendto(fd, bufSend, strlen(bufSend), 0, (struct sockaddr *) &nodes[nodes[(*messageToBeSent)->to].serverId].address, sizeof(nodes[nodes[(*messageToBeSent)->to].serverId].address)) < 0) {
							ERROR("sendto() failed @ %s:%d", __FILE__, __LINE__);
						} else {
							DBG("to='%s', req='%s'", nodeAddressPretty(&nodes[nodes[(*messageToBeSent)->to].serverId]), bufSend);
						}
					}
				}

				messageToBeSent = &(*messageToBeSent)->next;
			}
		}

		DBG("eventloop end");
	}

	/*
	 * CLEAN-UP
	 */
	close(fd);

	return 0;

}
