#include <stdio.h>
#include <stdlib.h>

#include "proto.h"
#include "net.h"

int main(int argc, char **argv)
{
	int sockfd;
	long version;
	struct connection conn;

	if (argc != 3) {
		fprintf(stderr, "USAGE: %s address port\n", argv[0]);
		return 2;
	}

	sockfd = net_connect(argv[1], atoi(argv[2]));
	if (sockfd < 0) {
		perror("net_connect");
		return 1;
	}

	proto_init(&conn, sockfd,
	           "client.example.net",
	           "deadbeef-deadbeef-deadbeef");

	if (client_helo(&conn) != 0) {
		fprintf(stderr, "client_helo returned non-zero\n");
		exit(1);
	}
	printf(">> Server knows who we are\n");

	if (client_query(&conn) != 200) {
		client_bye(&conn);
		exit(1);
	}

	printf("Server has version '%li'; newer than ours\n", conn.version);

	client_bye(&conn);
	return 0;

}
