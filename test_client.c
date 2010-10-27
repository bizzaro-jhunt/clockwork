#include <stdio.h>
#include <stdlib.h>

#include "proto.h"

#define CIPHERS "ALL:!ADM:!LOW"
#define CAFILE "certs/CA/cacert.pem"
#define CADIR  NULL

#define CERTFILE "certs/CA/certs/client.niftylogic.net.pem"
#define KEYFILE  "certs/client.niftylogic.net/key.pem"

#define SERVER "cfm.niftylogic.net"
#define PORT   "7890"

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
	SSL *ssl;
	SSL_CTX *ctx;
	long err;
	protocol_session session;
	uint32_t version;

	init_openssl();
	RAND_load_file("/dev/urandom", 1024);

	ctx = protocol_ssl_context(CAFILE, CERTFILE, KEYFILE);
	if (!ctx) {
		int_error("Error setting up SSL context");
	}

	sock = BIO_new_connect(SERVER ":" PORT);
	if (!sock) {
		int_error("Error creating connection BIO");
	}

	if (BIO_do_connect(sock) <= 0) {
		int_error("Error connecting to remote server");
	}

	if (!(ssl = SSL_new(ctx))) {
		int_error("Error creating an SSL context");
	}
	SSL_set_bio(ssl, sock, sock);
	if (SSL_connect(ssl) <= 0) {
		int_error("Error connecting SSL object");
	}
	if ((err = protocol_ssl_post_connection_check(ssl, SERVER)) != X509_V_OK) {
		fprintf(stderr, "-Error: peer certificate: %s\n", X509_verify_cert_error_string(err));
		int_error("Error checking SSL object ater connection");
	}

	fprintf(stderr, "SSL Connection opened\n");
	protocol_session_init(&session, ssl);

	version = client_get_policy_version(&session);
	if (version == 0) {
		client_disconnect(&session);
		SSL_clear(ssl);
		exit(1);
	}
	fprintf(stderr, ">> server has version %lu; newer than ours\n", (long unsigned int)version);
	client_disconnect(&session);

	SSL_shutdown(ssl);
	protocol_session_deinit(&session);

	SSL_free(ssl);
	SSL_CTX_free(ctx);
	return 0;
}

