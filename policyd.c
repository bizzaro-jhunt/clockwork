#include "clockwork.h"

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

#include <signal.h>

#include <unistd.h>
#include <getopt.h>

#define EXIT_BAD_CONFIG   100
#define EXIT_BAD_MANIFEST 101

/* Default config file; overridden by the -c argument */
//#define POLICYD_DOT_CONF "/etc/clockwork/policyd.conf"
#define POLICYD_DOT_CONF "policyd.conf"

/**
  A server thread.
 */
struct server {
	/** Duplex socket for communication with the connected client. */
	BIO         *socket;
	/** SSL handle object. */
	SSL         *ssl;
	/** SSL Context, containing encryption parameters. */
	SSL_CTX     *ssl_ctx;
	/** Thread ID of this server thread. */
	THREAD_TYPE  tid;
};

/**
  Options for the policy master daemon.
 */
struct options {
	/** Whether or not to daemonize the server process. */
	int   daemonize;
	/** Enable debug mode (if set to 1). */
	int   debug;
	/** Level of verbosity for log messages. */
	int   verbosity;

	/** Path to configuration file (policyd.conf) */
	char *config_path;
	/** TCP Port to listen on. */
	char *port;
};

/**************************************************************/

struct hash     *config = NULL;
pthread_mutex_t  config_mutex = PTHREAD_MUTEX_INITIALIZER;

struct manifest *manifest = NULL;
pthread_mutex_t  manifest_mutex = PTHREAD_MUTEX_INITIALIZER;

static void evaluate_arguments(struct options *opts, int argc, char **argv);
static void show_help(void);

static void server_setup(struct server *s);
static void server_loop(struct server *s);
static void server_teardown(struct server *s);

static void* server_thread(void *arg);
static void* policy_manager_thread(void *arg);

static void server_dispatch(protocol_session *session);

static void reread_config(void);
static void reread_manifest(void);

void sig_hup_handler(int, siginfo_t*, void*);

/**************************************************************/

int main(int argc, char **argv)
{
	struct server s;
	struct sigaction sig;
	struct daemon d;

	struct options opts = {
		.daemonize   = 1,
		.debug       = 0,
		.verbosity   = 3,
		.config_path = strdup(POLICYD_DOT_CONF),
		.port        = strdup("7890")
	};

	evaluate_arguments(&opts, argc, argv);

	config = parse_config(opts.config_path);
	if (!config) {
		ERROR("Unable to stat %s: %s", opts.config_path, strerror(errno));
		exit(EXIT_BAD_CONFIG);
	}

	errno = 0;
	manifest = parse_file(config_manifest_file(config));
	if (!manifest) {
		if (errno != 0) {
			ERROR("Unable to parse manifest file %s: %s", config_manifest_file(config), strerror(errno));
		} else {
			ERROR("Unable to parse manifest file %s: bad reference", config_manifest_file(config));
		}
		exit(EXIT_BAD_MANIFEST);
	}

	if (!opts.debug) {
		log_init("policyd");
	}
	server_setup(&s);
	INFO("Clockwork policyd starting up");

	/* Set up signal handlers */
	INFO("Setting up signal handlers");
	sig.sa_sigaction = sig_hup_handler;
	sig.sa_flags = SA_SIGINFO;
	if (sigaction(SIGHUP, &sig, NULL) != 0) {
		ERROR("Unable to set signal handlers: %s", strerror(errno));
	}

	if (opts.daemonize == 1) {
		d.lock_file = config_lock_file(config);
		d.pid_file  = config_pid_file(config);

		daemonize(&d);
	}

	/* Bind socket */
	/* FIXME: support for CLI -p flag */
	s.socket = BIO_new_accept( strdup(config_port(config)) );
	if (!s.socket || BIO_do_accept(s.socket) <= 0) {
		CRITICAL("Error binding server socket");
		protocol_ssl_backtrace();
		exit(2);
	}

	INFO("managing X policies for Y hosts");
	server_loop(&s);
	server_teardown(&s);

	return 0;
}

/**************************************************************/

static void evaluate_arguments(struct options *opts, int argc, char **argv)
{
	const char *short_opts = "h?FDvqQc:p:";
	struct option long_opts[] = {
		{ "help",         no_argument,       NULL, 'h' },
		{ "no-daemonize", no_argument,       NULL, 'F' },
		{ "foreground",   no_argument,       NULL, 'F' },
		{ "debug",        no_argument,       NULL, 'D' },
		{ "silent",       no_argument,       NULL, 'Q' },
		{ "config",       required_argument, NULL, 'c' },
		{ "port",         required_argument, NULL, 'p' },
		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1 ) {
		switch (opt) {
		case 'h':
		case '?':
			show_help();
			exit(0);
		case 'F':
			opts->daemonize = 0;
			break;
		case 'D':
			opts->debug = 1;
			opts->daemonize = 0; /* implies -F */
			break;
		case 'v':
			opts->verbosity++;
			break;
		case 'q':
			opts->verbosity--;
			break;
		case 'Q':
			opts->verbosity = 3;
			break;
		case 'c':
			free(opts->config_path);
			opts->config_path = strdup(optarg);
			break;
		case 'p':
			free(opts->port);
			opts->port = strdup(optarg);
			break;
		}
	}
}

static void show_help(void)
{
	printf( "USAGE: policyd [OPTIONS]\n"
	       "\n"
	       "  -h, --help            Show this helpful message.\n"
	       "                        (for more in-depth help, check the man pages.)\n"
	       "\n"
	       "  -F, --foreground      Do not fork into the background.\n"
	       "                        Useful for debugging with -D\n"
	       "\n"
	       "  -D, --debug           Run in debug mode; log to stderr instead of\n"
	       "                        syslog.  Implies -F.\n"
	       "\n"
	       "  -v[vvv...]            Increase verbosity by one level.  Can be used\n"
	       "                        more than once.  See -Q and -q.\n"
	       "\n"
	       "  -q[qqq...]            Decrease verbosity by one level.  Can be used\n"
	       "                        more than once.  See -v and -Q.\n"
	       "\n"
	       "  -Q, --silent          Reset verbosity to the default setting, such that\n"
	       "                        EMERGENCY, ALERT and CRITICAL messages are logged,\n"
	       "                        and all others are discarded.  See -q and -v.\n"
	       "\n"
	       "  -c, --config          Specify the path to an alternate configuration file.\n"
	       "                        If not given, defaults to " POLICYD_DOT_CONF "\n"
	       "\n"
	       "  -p, --port            Override the TCP port number that policyd should\n"
	       "                        bind to and listen on.\n"
	       "\n");
}

static void server_setup(struct server *s)
{
	assert(s);

	protocol_ssl_init();
	s->ssl_ctx = protocol_ssl_default_context(
			config_ca_cert_file(config),
			config_ca_cert_file(config),
			config_ca_key_file(config));
	if (!s->ssl_ctx) {
		CRITICAL("SSL: error initializing SSL");
		protocol_ssl_backtrace();
		exit(1);
	}
}

static void server_loop(struct server *s)
{
	assert(s);
	BIO *conn;

	for (;;) {
		if (BIO_do_accept(s->socket) <= 0) {
			WARNING("Couldn't accept inbound connection");
			protocol_ssl_backtrace();
			continue;
		}

		conn = BIO_pop(s->socket);
		if (!(s->ssl = SSL_new(s->ssl_ctx))) {
			WARNING("Error creating SSL handler for inbound connection");
			protocol_ssl_backtrace();
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

static void* server_thread(void *arg)
{
	SSL* ssl = (SSL*)arg;
	protocol_session session;
	long err;
	int sock;
	char addr[256];

	pthread_detach(pthread_self());

	if (SSL_accept(ssl) <= 0) {
		ERROR("Error establishin SSL connection");
		protocol_ssl_backtrace();
		return NULL;
	}

	sock = SSL_get_fd(ssl);
	INFO("incoming SSL connection on socket %i", sock);
	if (protocol_reverse_lookup_verify(sock, addr, 256) != 0) {
		ERROR("FCrDNS lookup (ipv4) failed: %s", strerror(errno));
		return NULL;
	}
	INFO("connection on socket %u from '%s'", sock, addr);

	if ((err = protocol_ssl_verify_peer(ssl, addr)) != X509_V_OK) {
		if (err == X509_V_ERR_APPLICATION_VERIFICATION) {
			ERROR("Peer certificate FQDN did not match FCrDNS lookup");
		} else {
			ERROR("SSL: problem with peer certificate: %s", X509_verify_cert_error_string(err));
			protocol_ssl_backtrace();
		}
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

static void server_dispatch(protocol_session *session)
{
	char errbuf[256] = {0};
	struct stree  *stree;
	struct policy *pol;
	struct hash   *facts;
	char *packed;

	for (;;) {
		pdu_receive(session);
		switch (RECV_PDU(session)->op) {

		case PROTOCOL_OP_BYE:
			return;

		case PROTOCOL_OP_GET_POLICY:
			facts = hash_new();
			if (!facts) {
				CRITICAL("Unable to allocate memory for facts hash");
				exit(42);
			}

			if (pdu_decode_GET_POLICY(RECV_PDU(session), facts) != 0) {
				CRITICAL("Unable to decode GET_POLICY");
				exit(42);
			}

			stree = hash_get(session->manifest->hosts, session->fqdn);
			if (!stree) {
				CRITICAL("No such host... %s", session->fqdn);
				exit(42);
			}

			pol = policy_generate(stree, facts);
			if (!pol) {
				CRITICAL("Unable to generate policy for host %s", session->fqdn);
				exit(42);
			}

			packed = policy_pack(pol);
			if (!packed) {
				CRITICAL("Unable to pack policy into a string");
				exit(42);
			}

			if (pdu_send_SEND_POLICY(session, pol) < 0) {
				CRITICAL("Unable to send SEND_POLICY");
				exit(42);
			}

			free(packed);
			pol = NULL; stree = NULL;
			hash_free(facts);
			break;

		case PROTOCOL_OP_PUT_REPORT:
			if (pdu_send_ACK(session) < 0) {
				CRITICAL("Unable to send ACK");
				exit(42);
			}
			break;

		default:
			snprintf(errbuf, 256, "Unrecognized PDU OP: %u", RECV_PDU(session)->op);
			if (pdu_send_ERROR(session, 405, (uint8_t*)errbuf, strlen(errbuf)) < 0) {
				CRITICAL("Unable to send ERROR");
				exit(42);
			}
			return;
		}

#if 0
	} else if (strcmp(session->line, "REPORT") == 0) {
		network_printf(session->nbuf, "301 Go Ahead\r\n");
		__proto_readline(session);
		while (strcmp(session->line, "DONE") != 0) {
			__proto_readline(session);
		}
		network_printf(session->nbuf, "200 OK\r\n");
		return 0;

	}
#endif
	} /* for (;;) */
}

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

void sig_hup_handler(int signum, siginfo_t *info, void *u)
{
	pthread_t tid;
	void *status;

	DEBUG("sig_hup_handler entered");

	pthread_create(&tid, NULL, policy_manager_thread, NULL);
	pthread_join(tid, &status);
}

