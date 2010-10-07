#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proto.h"
#include "net.h"

#define write_line(fd, s) write(fd, s "\r\n", strlen(s)+2)

int main(int argc, char **argv)
{
	int sockfd;
	char *server;
	unsigned short port;
	netbuf buf;
	char line[8192] = {0};
	size_t len = 0;
	long version;

	if (argc != 3) {
		fprintf(stderr, "USAGE: %s address port\n", argv[0]);
		return 2;
	}

	server = argv[1];
	port = atoi(argv[2]);

	sockfd = net_connect(server, port);
	if (sockfd < 0) {
		perror("net_connect");
		return 1;
	}

	netbuf_init(&buf, sockfd);

	if (client_helo(&buf, "client.example.net", "deadbeef-deadbeef-deadbeef") != 0) {
		fprintf(stderr, "client_helo returned non-zero\n");
		exit(1);
	}
	printf(">> Server knows who we are\n");

	if (client_query(&buf, &version) != 200) {
		client_bye(&buf);
		exit(1);
	}

	printf("Server has version '%li'; newer than ours\n", version);

	client_bye(&buf);
	return 0;

}
