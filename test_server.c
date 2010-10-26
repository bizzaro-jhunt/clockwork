#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

#include <openssl/bio.h>

#include "threads.h"
#include "proto.h"
#include "net.h"

#include <stdarg.h>

#define CAFILE "certs/CA/cacert.pem"
#define CADIR  NULL

#define CERTFILE "certs/CA/cacert.pem"
#define KEYFILE "certs/CA/private/cakey.pem"
#define CN "cfm.niftylogic.net"
#define CLIENT "client1.niftylogic.net"

#define int_error(msg) handle_error(__FILE__, __LINE__, msg)
void handle_error(const char *file, int lineno, const char *msg)
{
	fprintf(stderr, "** %s:%i %s\n", file, lineno, msg);
	ERR_print_errors_fp(stderr);
	exit(1);
}

SSL_CTX *setup_server_context(void)
{
	SSL_CTX *ctx;

	ctx = SSL_CTX_new(TLSv1_method());
	if (SSL_CTX_load_verify_locations(ctx, CAFILE, CADIR) != 1) {
		int_error("Error loading CA file / directory");
	}
	if (SSL_CTX_use_certificate_chain_file(ctx, CERTFILE) != 1) {
		int_error("Error loading certificate from file");
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, KEYFILE, SSL_FILETYPE_PEM) != 1) {
		int_error("Error loading private key from file");
	}
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
	SSL_CTX_set_verify_depth(ctx, 4);

	return ctx;
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

	if ((err = post_connection_check(ssl, CLIENT)) != X509_V_OK) {
		fprintf(stderr, "-Error: peer certificate: %s\n", X509_verify_cert_error_string(err));
		int_error("Error checking SSL object after connection");
	}

	fprintf(stderr, "SSL Connection opened.\n");
	protocol_session_init(&session, ssl);

	server_dispatch(&session);

	SSL_shutdown(ssl);

	fprintf(stderr, "Connection closed.\n");

	SSL_free(ssl);
	ERR_remove_state(0);
}

int main(int argc, char **argv)
{
	BIO *listen, *client;
	SSL *ssl;
	SSL_CTX *ctx;
	THREAD_TYPE tid;

	init_openssl();
	RAND_load_file("/dev/urandom", 1024);

	ctx = setup_server_context();

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
