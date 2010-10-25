#include <stdio.h>
#include <stdlib.h>

#include "proto.h"
#include "net.h"

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

SSL_CTX *setup_client_ssl_context(void)
{
	SSL_CTX *ctx;

	ctx = SSL_CTX_new(TLSv1_method());
	if (SSL_CTX_load_verify_locations(ctx, CAFILE, CADIR) != 1) {
		int_error("Error loading CA file / directory");
	}
	if (SSL_CTX_use_certificate_chain_file(ctx, CERTFILE) != 1) {
		int_error("Error loading ceritifcate from file");
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, KEYFILE, SSL_FILETYPE_PEM) != 1) {
		int_error("Error loading private key from file");
	}
	SSL_CTX_set_verify(ctx, SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
	SSL_CTX_set_verify_depth(ctx, 4);

	return ctx;
}

int main(int argc, char **argv)
{
	BIO *sock;
	SSL *ssl;
	SSL_CTX *ctx;
	long err;
	struct protocol_context pctx;

	init_openssl();
	RAND_load_file("/dev/urandom", 1024);

	ctx = setup_client_ssl_context();

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
	if ((err = post_connection_check(ssl, SERVER)) != X509_V_OK) {
		fprintf(stderr, "-Error: peer certificate: %s\n", X509_verify_cert_error_string(err));
		int_error("Error checking SSL object ater connection");
	}

	fprintf(stderr, "SSL Connection opened\n");
	proto_init(&pctx, ssl);

	if (client_query(&pctx) != 200) {
		client_bye(&pctx);
		SSL_clear(ssl);
		exit(1);
	}
//	fprintf(stderr, ">> server has version %li; newer than ours\n", pctx.version);
	client_bye(&pctx);
	SSL_clear(ssl);

	SSL_free(ssl);
	SSL_CTX_free(ctx);
	return 0;
}

