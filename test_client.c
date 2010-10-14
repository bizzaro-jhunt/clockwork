#include <stdio.h>
#include <stdlib.h>

#include "proto.h"
#include "net.h"

#define int_error(msg) handle_error(__FILE__, __LINE__, msg)
void handle_error(const char *file, int lineno, const char *msg)
{
	fprintf(stderr, "** %s:%i %s\n", file, lineno, msg);
	ERR_print_errors_fp(stderr);
	exit(1);
}

int main(int argc, char **argv)
{
	BIO *sock;
	network_buffer nbuf;
	struct connection conn;

	init_openssl();

	sock = BIO_new_connect("127.0.0.1:7890");
	if (!sock) {
		int_error("Error creating connection BIO");
	}

	if (BIO_do_connect(sock) <= 0) {
		int_error("Error connecting to remote server");
	}

	fprintf(stderr, "Connection opened\n");
	network_buffer_init(&nbuf, sock);

	proto_init(&conn, &nbuf,
	           "client.example.net",
	           "deadbeef-deadbeef-deadbeff");
	conn.version = 42;

	if (client_helo(&conn) != 0) {
		fprintf(stderr, "client_helo returned non-zero\n");
		exit(1);
	}
	fprintf(stderr, ">> server knows who we are\n");

	if (client_query(&conn) != 200) {
		client_bye(&conn);
		exit(1);
	}
	fprintf(stderr, ">> server has version %li; newer than ours\n", conn.version);
	client_bye(&conn);

	BIO_free(sock);
	return 0;
}

