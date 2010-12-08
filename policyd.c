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

#include <signal.h>

/* Default config file; overridden by the -c argument */
//#define POLICYD_DOT_CONF "/etc/clockwork/policyd.conf"
#define POLICYD_DOT_CONF "policyd.conf"

#define SA(s) (struct sockaddr*)(s)

/**************************************************************/

/* just an idea at this point... */
struct server {
	BIO         *socket;
	SSL         *ssl;
	SSL_CTX     *ssl_ctx;
	THREAD_TYPE  tid;
};

struct hash     *config = NULL;
pthread_mutex_t  config_mutex = PTHREAD_MUTEX_INITIALIZER;
struct manifest *manifest = NULL;
pthread_mutex_t  manifest_mutex = PTHREAD_MUTEX_INITIALIZER;

static void server_setup(struct server *s);
static void server_bind(struct server *s);
static void server_loop(struct server *s);
static void server_teardown(struct server *s);

#define int_error(msg) handle_error(__FILE__, __LINE__, msg)
static void handle_error(const char *file, int lineno, const char *msg);
static void* server_thread(void *arg);
static void* policy_manager_thread(void *arg);

static void reread_config()
{
	struct hash *new_config;

	new_config = parse_config(POLICYD_DOT_CONF);
	if (new_config) {
		DEBUG("%s:%u in %s: updating global config", __FILE__, __LINE__, __func__);

		pthread_mutex_lock(&config_mutex);
		config = new_config;
		pthread_mutex_unlock(&config_mutex);
	} else {
		WARNING("Unable to stat %s: %s", POLICYD_DOT_CONF, strerror(errno));
	}
}

static void reread_manifest()
{
	struct manifest *new_manifest;

	pthread_mutex_lock(&config_mutex);
	new_manifest = parse_file(config_manifest_file(config));
	pthread_mutex_unlock(&config_mutex);

	if (new_manifest) {
		DEBUG("%s:%u in %s: updating global manifest", __FILE__, __LINE__, __func__);

		pthread_mutex_lock(&manifest_mutex);
		manifest = new_manifest;
		pthread_mutex_unlock(&manifest_mutex);
	} else {
		WARNING("Unable to parse manifest... Leaving existing manifest in place");
	}
}

void sig_hup_handler(int, siginfo_t*, void*);

/**************************************************************/

#define EXIT_BAD_CONFIG   100
#define EXIT_BAD_MANIFEST 101

int main(int argc, char **argv)
{
	struct server s;
	struct sigaction sig;
	struct daemon d;

	config = parse_config(POLICYD_DOT_CONF);
	if (!config) {
		fprintf(stderr, "error: unable to stat %s: %s\n", POLICYD_DOT_CONF, strerror(errno));
		exit(EXIT_BAD_CONFIG);
	}

	manifest = parse_file(config_manifest_file(config));
	if (!manifest) {
		fprintf(stderr, "error: unable to parse manifest file %s: %s\n", config_manifest_file(config), strerror(errno));
		exit(EXIT_BAD_MANIFEST);
	}

	server_setup(&s);
	INFO("Clockwork policyd starting up");

	INFO("Setting up signal handlers");
	/* Set up signal handlers */
	sig.sa_sigaction = sig_hup_handler;
	sig.sa_flags = SA_SIGINFO;
	if (sigaction(SIGHUP, &sig, NULL) != 0) {
		ERROR("Unable to set signal handlers: %s", strerror(errno));
	}

	d.lock_file = config_lock_file(config);
	d.pid_file  = config_pid_file(config);

	//daemonize(&d);
	server_bind(&s);
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
			config_ca_cert_file(config),
			config_ca_cert_file(config),
			config_ca_key_file(config));
	if (!s->ssl_ctx) {
		int_error("Error setting up SSL context");
	}
}

static void server_bind(struct server *s)
{
	assert(s);

	s->socket = BIO_new_accept( strdup(config_port(config)) );
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

	if (getpeername(sockfd, SA(&ipv4), &ipv4_len) != 0
	 || getnameinfo(SA(&ipv4), ipv4_len, addr, len, NULL, 0, NI_NAMEREQD) != 0) {
		return -1;
	}

	return 0;
}

static void* server_thread(void *arg)
{
	SSL* ssl = (SSL*)arg;
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

	protocol_session_init(&session, ssl, addr);
	session.manifest = manifest;
	server_dispatch(&session);

	SSL_shutdown(ssl);
	INFO("SSL connection closed");

	protocol_session_deinit(&session);
	SSL_free(ssl);
	ERR_remove_state(0);

	return NULL; /* FIXME: what should we be returning? */
}

/**
   The policy manager thread is responsible for reloading the manifest
   (collection of all host and policy definitions) for other session
   threads.

   First, the manifest is re-parsed into a temporary manifest structure.
   If parsing succeeds, then a lock is acquired on the manifest_mutex,
   and the global manifest is replaced with the new manifest.  If parsing
   fails, the update is not performed.  In either case, the mutex lock
   is released.
 */
static void* policy_manager_thread(void *arg)
{
	INFO("Caught SIGHUP; reloading config and manifest");

	reread_config();
	reread_manifest();
	return NULL;
}

void sig_hup_handler(int signum, siginfo_t *info, void *u)
{
	pthread_t tid;
	void *status;

	DEBUG("sig_hup_handler entered");

	pthread_create(&tid, NULL, policy_manager_thread, NULL);
	pthread_join(tid, &status);
}

