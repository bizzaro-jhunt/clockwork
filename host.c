#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SA(s) (struct sockaddr*)(s)

int fcrlookup(const char *lookup)
{
	struct addrinfo *addr_list, hint;
	char host_name[256];
	struct addrinfo *ptr;
	char address[INET6_ADDRSTRLEN];

	memset(&hint, 0, sizeof(hint));
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;

	struct sockaddr_in  *ipv4;
	struct sockaddr_in6 *ipv6;

	if (getaddrinfo(lookup, NULL, &hint, &addr_list) != 0) {
		perror("Unable to getaddrinfo");
		exit(2);
	}

	for (ptr = addr_list; ptr; ptr = ptr->ai_next) {
		int rc;

		switch (ptr->ai_family) {
		case AF_INET:
			ipv4 = (struct sockaddr_in*)(ptr->ai_addr);
			inet_ntop(ptr->ai_family, &ipv4->sin_addr, address, sizeof(address));
			rc = getnameinfo(SA(ipv4), sizeof(struct sockaddr_in),
				host_name, 256, NULL, 0, NI_NAMEREQD);
			break;

		case AF_INET6:
			ipv6 = (struct sockaddr_in6*)(ptr->ai_addr);
			inet_ntop(ptr->ai_family, &ipv6->sin6_addr, address, sizeof(address));
			rc = getnameinfo(SA(ipv6), sizeof(struct sockaddr_in6),
				host_name, 256, NULL, 0, NI_NAMEREQD);
			break;

		default:
			fprintf(stderr, "Unknown addrinfo family: %u\n", ptr->ai_family);
			rc = -1;
		}

		if (rc != 0) {
			perror("Unable to getnameinfo");
			exit(3);
		}

		if (strcmp(lookup, host_name) == 0) {
			printf("GOOD: %s == %s\t%s\n", lookup, host_name, address);
		} else {
			printf("BAD:  %s != %s\t%s\n", lookup, host_name, address);
		}
	}

	return 0;
}

int main(int argc, char **argv)
{

	int i;

	if (argc < 2) {
		fprintf(stderr, "USAGE: %s hostname\n", argv[0]);
		return 2;
	}

	for (i = 1; i < argc; i++) {
		fcrlookup(argv[i]);
	}

	return 0;
}
