#include "policy.h"
#include "spec/parser.h"
#include "config/parser.h"
#include "threads.h"
#include "proto.h"
#include "log.h"
#include "daemon.h"
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

/* Default config file; overridden by the -c argument */
//#define POLICYD_DOT_CONF "/etc/clockwork/policyd.conf"
#define POLICYD_DOT_CONF "policyd.conf"

#define SA(s) (struct sockaddr*)(s)

/**************************************************************/

/* just an idea at this point... */
struct server {
	struct manifest *manifest;
	struct hash *config;

	BIO         *socket;
	SSL         *ssl;
	SSL_CTX     *ssl_ctx;
	THREAD_TYPE  tid;
};

static void server_setup(struct server *s);
static void server_bind(struct server *s);
static void server_loop(struct server *s);
static void server_teardown(struct server *s);

#define int_error(msg) handle_error(__FILE__, __LINE__, msg)
static void handle_error(const char *file, int lineno, const char *msg);
static void* server_thread(void *arg);

/**************************************************************/

#define EXIT_BAD_CONFIG   100
#define EXIT_BAD_MANIFEST 101

int main(int argc, char **argv)
{
	struct server s;
	struct daemon d;

	s.config   = parse_config(POLICYD_DOT_CONF);
	if (!s.config) {
		fprintf(stderr, "error: unable to stat %s: %s\n", POLICYD_DOT_CONF, strerror(errno));
		exit(EXIT_BAD_CONFIG);
	}
	s.manifest = parse_file(config_manifest_file(s.config));
	if (!s.manifest) {
		fprintf(stderr, "error: unable to parse manifest file %s: %s\n", config_manifest_file(s.config), strerror(errno));
		exit(EXIT_BAD_MANIFEST);
	}

	d.lock_file = config_lock_file(s.config);
	d.pid_file  = config_pid_file(s.config);

	server_setup(&s);

	daemonize(&d);
	server_bind(&s);
	INFO("Clockwork policyd starting up");
	INFO("managing X policies for Y hosts");
	server_loop(&s);
	server_teardown(&s);

	return 0;
}

/**************************************************************/

static void server_setup(struct server *s)
{
	assert(s);

	log_init("policyd");
	protocol_ssl_init();
	s->ssl_ctx = protocol_ssl_default_context(
			config_ca_cert_file(s->config),
			config_ca_cert_file(s->config),
			config_ca_key_file(s->config));
	if (!s->ssl_ctx) {
		int_error("Error setting up SSL context");
	}
}

static void server_bind(struct server *s)
{
	assert(s);

	s->socket = BIO_new_accept(config_port(s->config));
	if (!s->socket) {
		int_error("Error creating server socket");
		exit(2);
	}

	if (BIO_do_accept(s->socket) <= 0) {
		int_error("Error binding server socket");
		exit(2);
	}
}

static void server_loop(struct server *s)
{
	assert(s);
	BIO *conn;

	for (;;) {
		if (BIO_do_accept(s->socket) <= 0) {
			int_error("Error accepting connection");
			continue;
		}

		conn = BIO_pop(s->socket);
		if (!(s->ssl = SSL_new(s->ssl_ctx))) {
			int_error("Error creating SSL context");
			continue;
		}

		SSL_set_bio(s->ssl, conn, conn);
		THREAD_CREATE(s->tid, server_thread, s->ssl);
	}
}

static void server_teardown(struct server *s)
{
	assert(s);

	SSL_CTX_free(s->ssl_ctx);
	BIO_free(s->socket);
}

static void handle_error(const char *file, int lineno, const char *msg)
{
	fprintf(stderr, "** %s:%i %s\n", file, lineno, msg);
	ERR_print_errors_fp(stderr); /* FIXME: send to syslog somehow */
	exit(1);
}

static int fcrdns_ipv4_lookup(int sockfd, char *addr, size_t len)
{
	struct sockaddr_in ipv4;
	socklen_t ipv4_len;

	ipv4_len = sizeof(ipv4);
	memset(&ipv4, 0, ipv4_len);

	if (getpeername(sockfd, SA(&ipv4), &ipv4_len) != 0) {
		return -1;
	}

	/*
	if (!inet_ntop(AF_INET, &ipv4.sin_addr, addr, len)) {
		return -1;
	}
	*/

	if (getnameinfo(SA(&ipv4), ipv4_len, addr, len, NULL, 0, NI_NAMEREQD) != 0) {
		return -1;
	}

	return 0;
}

static void* server_thread(void *arg)
{
	SSL *ssl = (SSL*)arg;
	protocol_session session;
	long err;
	int sock;
	char addr[256];

	pthread_detach(pthread_self());

	if (SSL_accept(ssl) <= 0) {
		int_error("Error accepting SSL connection");
	}

	sock = SSL_get_fd(ssl);
	INFO("incoming SSL connection on socket %i", sock);
	if (fcrdns_ipv4_lookup(sock, addr, 256) != 0) {
		ERROR("fcrdns_ipv4_lookup failed: %s", strerror(errno));
		return NULL;
	}
	INFO("connection on socket %u from '%s'", sock, addr);

	if ((err = protocol_ssl_verify_peer(ssl, addr)) != X509_V_OK) {
		if (err == X509_V_ERR_APPLICATION_VERIFICATION) {
			ERROR("peer certificate did not match fcrdns fqdn");
		} else {
			ERROR("problem with peer certificate: %s", X509_verify_cert_error_string(err));
		}
		ERR_print_errors_fp(stderr); /* FIXME: send to syslog somehow */
		SSL_clear(ssl);

		SSL_free(ssl);
		ERR_remove_state(0);
		return NULL;
	}

	protocol_session_init(&session, ssl);
	server_dispatch(&session);

	SSL_shutdown(ssl);
	INFO("SSL connection closed");

	protocol_session_deinit(&session);
	SSL_free(ssl);
	ERR_remove_state(0);

	return NULL; /* FIXME: what should we be returning? */
}

