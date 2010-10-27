#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "threads.h"
#include "proto.h"


#define CAFILE   "certs/CA/cacert.pem"
#define CERTFILE "certs/CA/cacert.pem"
#define KEYFILE  "certs/CA/private/cakey.pem"
#define CLIENT   "client1.niftylogic.net"

#define int_error(msg) handle_error(__FILE__, __LINE__, msg)
void handle_error(const char *file, int lineno, const char *msg)
{
	fprintf(stderr, "** %s:%i %s\n", file, lineno, msg);
	ERR_print_errors_fp(stderr);
	exit(1);
}

void* server_thread(void *arg)
{
	SSL *ssl = (SSL*)arg;
	protocol_session session;
	long err;

	pthread_detach(pthread_self());

	if (SSL_accept(ssl) <= 0) {
		int_error("Error accepting SSL connection");
	}

	if ((err = protocol_ssl_verify_peer(ssl, CLIENT)) != X509_V_OK) {
		fprintf(stderr, "-Error: peer certificate: %s\n", X509_verify_cert_error_string(err));
		int_error("Error checking SSL object after connection");
	}

	fprintf(stderr, "SSL Connection opened.\n");
	protocol_session_init(&session, ssl);

	server_dispatch(&session);

	SSL_shutdown(ssl);
	fprintf(stderr, "Connection closed.\n");

	protocol_session_deinit(&session);
	SSL_free(ssl);
	ERR_remove_state(0);
}

int main(int argc, char **argv)
{
	BIO *listen, *client;
	SSL *ssl;
	SSL_CTX *ctx;
	THREAD_TYPE tid;

	protocol_ssl_init();
	ctx = protocol_ssl_default_context(CAFILE, CERTFILE, KEYFILE);
	if (!ctx) {
		int_error("Error setting up SSL context");
	}

	listen = BIO_new_accept("7890");
	if (!listen) {
		int_error("Error creating server socket");
	}

	if (BIO_do_accept(listen) <= 0) {
		int_error("Error binding server socket");
	}

	fprintf(stderr, "Server ready for connections\n");

	for (;;) {
		if (BIO_do_accept(listen) <= 0) {
			int_error("Error accepting connection");
		}

		client = BIO_pop(listen);
		if (!(ssl = SSL_new(ctx))) {
			int_error("Error creating SSL context");
		}

		SSL_set_bio(ssl, client, client);
		THREAD_CREATE(tid, server_thread, ssl);
	}

	SSL_CTX_free(ctx);
	BIO_free(listen);
	return 0;
}
