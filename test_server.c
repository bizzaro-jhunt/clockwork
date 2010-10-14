#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

#include <openssl/bio.h>

#include "threads.h"
#include "proto.h"
#include "net.h"

#include <stdarg.h>

#define int_error(msg) handle_error(__FILE__, __LINE__, msg)
void handle_error(const char *file, int lineno, const char *msg)
{
	fprintf(stderr, "** %s:%i %s\n", file, lineno, msg);
	ERR_print_errors_fp(stderr);
	exit(1);
}

void do_server_loop(network_buffer *nbuf)
{
	char line[255] = { 0 };
	size_t len;

	while ( (len = network_readline(nbuf, line, 255)) > 0 ) {
		network_printf(nbuf, "ECHO: %s\r\n", line);
	}
}

void* server_thread(void *arg)
{
	BIO *client;
	network_buffer nbuf;
	struct connection conn;

	client = (BIO*)arg;
	network_buffer_init(&nbuf, client);

	pthread_detach(pthread_self());

	fprintf(stderr, "Connection opened.\n");

	proto_init(&conn, &nbuf,
	           "server.example.com",
	           "decafbad-decafbad-decafbad");
	server_dispatch(&conn);

	fprintf(stderr, "Connection closed.\n");

	BIO_free(client);
	ERR_remove_state(0);
}

int main(int argc, char **argv)
{
	BIO *listen, *client;
	THREAD_TYPE tid;

	listen = BIO_new_accept("7890");
	if (!listen) {
		int_error("Error creating server socket");
	}

	if (BIO_do_accept(listen) <= 0) {
		int_error("Error binding server socket");
	}

	for (;;) {
		if (BIO_do_accept(listen) <= 0) {
			int_error("Error accepting connection");
		}

		client = BIO_pop(listen);
		THREAD_CREATE(tid, server_thread, client);
	}

	BIO_free(listen);
	return 0;
}
