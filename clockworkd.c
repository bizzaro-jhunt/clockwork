#include "policy.h"
#include "spec/parser.h"
#include "threads.h"
#include "proto.h"
#include "log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//#define DEFAULT_POLICY_FILE "/etc/clockwork/policies.pol"
#define DEFAULT_POLICY_FILE "policy.pol"

/* FIXME: put these in configuration */
#define CONFIG_CAFILE   "certs/CA/cacert.pem"
#define CONFIG_CERTFILE "certs/CA/cacert.pem"
#define CONFIG_KEYFILE  "certs/CA/private/cakey.pem"

#define CONFIG_PORT     "7890"
#define CONFIG_LOCK_FILE "/var/lock/clockworkd"

/**************************************************************/

/* just an idea at this point... */
struct server {
	struct manifest *manifest;
	BIO         *socket;
	SSL         *ssl;
	SSL_CTX     *ssl_ctx;
	THREAD_TYPE  tid;
};

static void usage();
static void server_setup(struct server *s);
static void server_bind(struct server *s);
static void server_daemonize(struct server *s, const char *lock_file);
static void server_loop(struct server *s);
static void server_teardown(struct server *s);

#define int_error(msg) handle_error(__FILE__, __LINE__, msg)
static void handle_error(const char *file, int lineno, const char *msg);
static void* server_thread(void *arg);

/**************************************************************/

int main(int argc, char **argv)
{
	struct server s;

	s.manifest = parse_file(DEFAULT_POLICY_FILE);

	server_setup(&s);
	server_daemonize(&s, CONFIG_LOCK_FILE);
	server_bind(&s);
	server_loop(&s);
	server_teardown(&s);

	return 0;
}

/**************************************************************/

static void usage()
{
	printf("Usage: clockworkd\n");
}

static void server_setup(struct server *s)
{
	assert(s);

	log_init("clockworkd");
	protocol_ssl_init();
	s->ssl_ctx = protocol_ssl_default_context(CONFIG_CAFILE, CONFIG_CERTFILE, CONFIG_KEYFILE);
	if (!s->ssl_ctx) {
		int_error("Error setting up SSL context");
	}
}

static void server_bind(struct server *s)
{
	assert(s);

	s->socket = BIO_new_accept(CONFIG_PORT);
	if (!s->socket) {
		int_error("Error creating server socket");
		exit(2);
	}

	if (BIO_do_accept(s->socket) <= 0) {
		int_error("Error binding server socket");
		exit(2);
	}
}

static void server_daemonize(struct server *s, const char *lock)
{
	assert(s);

	pid_t pid, sid;
	int lock_fd = -1;

	if (getppid() == 1) {
		return; /* already a child of init */
	}

	/* Open the lock file. We don't need O_EXCL; we will lock the
	   file with a POSIX lockf(3) in the child process. */
	if (lock && lock[0]) {
		lock_fd = open(lock, O_CREAT | O_RDWR, 0640);
		if (lock_fd < 0) {
			ERROR("unable to open lock file %s: %s", lock, strerror(errno));
			exit(2);
		}
	}

	pid = fork();
	if (pid < 0) {
		ERROR("unable to fork: %s", strerror(errno));
		exit(2);
	}

	if (pid > 0) { /* parent process */
		_exit(0);
	}

	/* child process */

	if (lock_fd >= 0) { /* lock the lok */
		if (lockf(lock_fd, F_TLOCK, 0) < 0) {
			ERROR("unable to lock %s; daemon already running?", lock);
			exit(2);
		}
	}

	umask(0); /* reset the file umask */

	sid = setsid(); /* new process session / group */
	if (sid < 0 && errno != EPERM) {
		ERROR("unable to create new process group: %s", strerror(errno));
		exit(2);
	}

	if (chdir("/") < 0) {
		ERROR("unable to chdir to /: %s", strerror(errno));
		exit(2);
	}

	/* redirect standard fds */
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
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
	ERR_print_errors_fp(stderr);
	exit(1);
}

static void* server_thread(void *arg)
{
	SSL *ssl = (SSL*)arg;
	protocol_session session;
	long err;

	pthread_detach(pthread_self());

	if (SSL_accept(ssl) <= 0) {
		int_error("Error accepting SSL connection");
	}

	/* FIXME FcR IPv4 lookup here */
	if ((err = protocol_ssl_verify_peer(ssl, "cfm1.niftylogic.net")) != X509_V_OK) {
		fprintf(stderr, "-Error: peer certificate: %s\n", X509_verify_cert_error_string(err));
		/* send some sort of IDENT error back */
		ERR_print_errors_fp(stderr);
		SSL_clear(ssl);

		SSL_free(ssl);
		ERR_remove_state(0);
		return NULL;
	}

	INFO("SSL connection opened");
	protocol_session_init(&session, ssl);

	server_dispatch(&session);

	SSL_shutdown(ssl);
	INFO("SSL connection closed");

	protocol_session_deinit(&session);
	SSL_free(ssl);
	ERR_remove_state(0);

	return NULL; /* FIXME: what should we be returning? */
}

