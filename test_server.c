#include <stdio.h>
#include <stdlib.h>

#include <sys/wait.h>
#include <poll.h>

#include "proto.h"
#include "net.h"

int main(int argc, char **argv)
{
	int sockfd, connfd;
	struct connection *conn;
	char *server;
	unsigned short port;
	struct pollfd pfd;

	if (argc != 3) {
		fprintf(stderr, "USAGE: %s address port\n", argv[0]);
		return 2;
	}

	server = argv[1];
	port = atoi(argv[2]);

	sockfd = net_listen(server, port);
	if (sockfd < 0) {
		perror("net_listen");
		return 1;
	}

	pfd.fd = sockfd;
	pfd.events = POLL_IN;

	for (;;) {
		if (poll(&pfd, 1, 1) > 0) {
			connfd = net_accept(sockfd);
			switch (connfd) {
			case -1:
				perror("net_accept");
				return 1;
			case  0:
				fprintf(stderr, "parent: spun off a new connection\n");
				break;
			default:
				fprintf(stderr, "child: serving client\n");
				conn = malloc(sizeof(struct connection));
				proto_init(conn, connfd, "server.niftylogic.net", "decafbad-decafbad-decafbad");
				return server_dispatch(conn);
			}
		} else {
			waitpid(-1, NULL, WNOHANG);
		}
	}
	return 0;
}
