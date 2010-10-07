#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "net.h"

#define SA(s) ((struct sockaddr *)(s))

int net_listen(const char *address, uint16_t port)
{
	int fd;
	struct sockaddr_in sa;

	/* get a socket from the kernel */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) { return -1; }

	/* set up sa with address / port to listen on */
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port); /* in_port_t == uint16_t */

	errno = 0;
	if (inet_pton(AF_INET, address, &sa.sin_addr) != 1) {
		if (errno == 0) { errno = EINVAL; }
		return -1;
	}

	/* bind the socket to a listening address */
	if (bind(fd, SA(&sa), sizeof(sa)) != 0) {
		return -1;
	}

	/* listen on the bound socket */
	if (listen(fd, 0) != 0) {
		return -1;
	}

	return fd;
}

int net_accept(int fd)
{
	int conn;

	conn = accept(fd, NULL, NULL);
	if (conn < 0) { return -1; }

	switch (fork()) {
		case -1: return -1;
		case  0: /* in child */
			close(fd);
			return conn;
		default: /* in parent */
			close(conn);
			return 0;
	}
}

