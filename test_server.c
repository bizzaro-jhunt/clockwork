#include <stdio.h>
#include <stdlib.h>

#include "proto.h"
#include "net.h"

int main(int argc, char **argv)
{
	int sockfd, conn;
	char *server;
	unsigned short port;

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

	for (;;) {
		conn = net_accept(sockfd);
		switch (conn) {
		case -1:
			perror("net_accept");
			return 1;
		case  0:
			printf("spun off a new connection\n");
			break;
		default:
			return server_dispatch(conn);
		}
	}
	return 0;

}
