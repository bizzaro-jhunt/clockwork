#include "server.h"
#include "spec/parser.h"
#include "config/parser.h"

#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef struct {
	BIO *socket;
	SSL *ssl;
	THREAD_TYPE tid;

	char *peer;

	struct hash   *facts;
	struct policy *policy;
	struct stree  *pnode;

	protocol_session session;

	struct hash *config;
} worker;

/**************************************************************/

static const char      *manifest_file = NULL;
static struct manifest *manifest = NULL;
static pthread_mutex_t  manifest_mutex = PTHREAD_MUTEX_INITIALIZER;

/**************************************************************/

static worker* worker_spawn(server *s);
static int worker_prep(worker *w);
static int worker_dispatch(worker *w);
static void worker_die(worker *w);
static int worker_verify_peer(worker *w);
static int worker_send_file(worker *w, const char *path);

static void server_sighup(int, siginfo_t*, void*);

static void daemonize(const char *lock_file, const char *pid_file);
static void daemon_acquire_lock(const char *path);
static void daemon_fork1(void);
static void daemon_fork2(const char *path);
static void daemon_write_pid(pid_t pid, const char *path);
static void daemon_settle(void);

/**********************************************************/

static worker* worker_spawn(server *s)
{
	worker *w;

	if (BIO_do_accept(s->listener) <= 0) {
		WARNING("Couldn't accept inbound connection");
		protocol_ssl_backtrace();
		return NULL;
	}

	w = xmalloc(sizeof(worker));
	w->facts = hash_new();

	w->socket = BIO_pop(s->listener);
	if (!(w->ssl = SSL_new(s->ssl_ctx))) {
		WARNING("Couldn't create SSL handle for worker thread");
		protocol_ssl_backtrace();

		free(w);
		return NULL;
	}

	SSL_set_bio(w->ssl, w->socket, w->socket);
	return w;
}

static int worker_prep(worker *w)
{
	assert(w);

	sigset_t blocked_signals;

	/* block SIGPIPE for clients that close early. */
	sigemptyset(&blocked_signals);
	sigaddset(&blocked_signals, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &blocked_signals, NULL);

	if (SSL_accept(w->ssl) <= 0) {
		ERROR("Unable to establish SSL connection");
		protocol_ssl_backtrace();
		return -1;
	}

	if (worker_verify_peer(w) != 0) {
		return -1;
	}

	protocol_session_init(&w->session, w->ssl);

	pthread_mutex_lock(&manifest_mutex);
		w->pnode = hash_get(manifest->hosts, w->peer);
	pthread_mutex_unlock(&manifest_mutex);

	w->policy = NULL;
	return 0;
}

static int worker_dispatch(worker *w)
{
	assert(w);

	char *err;
	sha1 checksum; /* for PROTOCOL_OP_GET_FILE */
	struct res_file *rf, *rf_match; /* for PROTOCOL_OP_GET_FILE */

	protocol_session *sess = &w->session;

	for (;;) {
		pdu_receive(sess);
		switch (RECV_PDU(sess)->op) {

		case PROTOCOL_OP_BYE:
			pdu_send_ACK(sess);
			return 0;

		case PROTOCOL_OP_GET_POLICY:
			if (pdu_decode_GET_POLICY(RECV_PDU(sess), w->facts) != 0) {
				CRITICAL("Unable to decode GET_POLICY");
				return -2;
			}

			if (!w->policy && w->pnode) {
				w->policy = policy_generate(w->pnode, w->facts);
			}

			if (!w->policy) {
				CRITICAL("Unable to generate policy for host %s", w->peer);
				return -2;
			}

			if (pdu_send_SEND_POLICY(sess, w->policy) < 0) {
				CRITICAL("Unable to send SEND_POLICY");
				return -2;
			}

			break;

		case PROTOCOL_OP_GET_FILE:
			if (pdu_decode_GET_FILE(RECV_PDU(sess), &checksum) != 0) {
				CRITICAL("Unable to decode GET_FILE");
				return -2;
			}
			/* Search for the res_file in the policy */
			if (!w->policy) {
				CRITICAL("No policy generated YET");
				return -2;
			}

			DEBUG("GET_FILE: %s", checksum.hex);

			rf = NULL;
			for_each_node(rf_match, &w->policy->res_files, res) {
				DEBUG("Check '%s' against '%s'", rf_match->rf_rsha1.hex, checksum.hex);
				if (sha1_cmp(&rf_match->rf_rsha1, &checksum) == 0) {
					rf = rf_match;
					break;
				}
			}

			if (rf) {
				DEBUG("Found a res_file for %s: %s\n", checksum.hex, rf->rf_rpath);
				if (worker_send_file(w, rf->rf_rpath) != 0) {
					DEBUG("Unable to send file");
				}
			} else {
				DEBUG("Could not find a res_file for: %s\n", checksum.hex);
				if (worker_send_file(w, "/dev/null") != 0) {
					DEBUG("Unable to send null file");
				}
			}

			break;

		case PROTOCOL_OP_PUT_REPORT:
			if (pdu_send_ACK(sess) < 0) {
				CRITICAL("Unable to send ACK");
				return -2;
			}
			break;

		default:
			err = string("Unrecognized PDU OP: %u", RECV_PDU(sess)->op);
			WARNING("%s", err);
			if (pdu_send_ERROR(sess, 405, err) < 0) {
				CRITICAL("Unable to send ERROR");
			}
			free(err);
			return -1;
		}
	}
}

static void worker_die(worker *w)
{
	assert(w);

	SSL_shutdown(w->ssl);
	SSL_free(w->ssl);
	ERR_remove_state(0);

	free(w->peer);
	free(w);
}

static int worker_verify_peer(worker *w)
{
	assert(w);

	long err;
	int sock;
	char addr[256];

	sock = SSL_get_fd(w->ssl);
	if (protocol_reverse_lookup_verify(sock, addr, 256) != 0) {
		ERROR("FCrDNS lookup (ipv4) failed: %s", strerror(errno));
		return -1;
	}
	INFO("Connection on socket %u from '%s'", sock, addr);

	if ((err = protocol_ssl_verify_peer(w->ssl, addr)) != X509_V_OK) {
		if (err == X509_V_ERR_APPLICATION_VERIFICATION) {
			ERROR("Peer certificate FQDN did not match FCrDNS lookup");
		} else {
			ERROR("SSL: problem with peer certificate: %s", X509_verify_cert_error_string(err));
			protocol_ssl_backtrace();
		}
		SSL_clear(w->ssl);

		SSL_free(w->ssl);
		ERR_remove_state(0);
		return -1;
	}

	w->peer = strdup(addr);
	return 0;
}

static int worker_send_file(worker *w, const char *path)
{
	int fd = open(path, O_RDONLY);
	int n;
	while ((n = pdu_send_FILE_DATA(&w->session, fd)) > 0)
		;

	return n;
}

static void server_sighup(int signum, siginfo_t *info, void *udata)
{
	pthread_t tid;
	void *status;

	DEBUG("SIGHUP handler entered");

	pthread_create(&tid, NULL, server_manager_thread, NULL);
	pthread_join(tid, &status);
}

/**
  Daemonize the current process, by forking into the background,
  closing stdin, stdout and stderr, and detaching from the
  controlling terminal.

  The \a name, \a lock_file, and \a pid_file parameters allow
  the caller to control how the process is daemonized.

  @param  lock_file  Path to use as a file lock, to prevent multiple
                     daemon instances from runnin concurrently.
  @param  pid_file   Path to store the daemon's ultimate process ID.

  @returns to the caller if daemonization succeeded.  The parent
           process exits with a status code of 0.  If daemonization
           fails, this call will not return, and a the current
           process will exit non-zero.
 */
static void daemonize(const char *lock_file, const char *pid_file)
{
	/* are we already a child of init? */
	if (getppid() == 1) { return; }

	daemon_fork1();
	daemon_fork2(pid_file);
	daemon_acquire_lock(lock_file);
	daemon_settle();
}

static void daemon_acquire_lock(const char *path)
{

	int lock_fd;

	if (!(path && path[0])) {
		ERROR("NULL or empty lock file path given");
		exit(2);
	}

	lock_fd = open(path, O_CREAT | O_RDWR, 0640);
	if (lock_fd < 0) {
		ERROR("unable to open lock file %s: %s", path, strerror(errno));
		exit(2);
	}

	if (lockf(lock_fd, F_TLOCK, 0) < 0) {
		ERROR("unable to lock %s; daemon already running?", path);
		exit(2);
	}

}

static void daemon_fork1(void)
{
	pid_t pid = fork();

	if (pid < 0) {
		ERROR("unable to fork: %s", strerror(errno));
		exit(2);
	}

	if (pid > 0) { /* parent process */
		_exit(0);
	}
}

static void daemon_fork2(const char *path)
{
	pid_t pid, sessid;

	sessid = setsid(); /* new process session / group */
	if (sessid < 0) {
		ERROR("unable to create new process group: %s", strerror(errno));
		exit(2);
	}

	pid = fork();
	if (pid < 0) {
		ERROR("unable to fork again: %s", strerror(errno));
		exit(2);
	}

	if (pid > 0) { /* "middle" child / parent */
		/* Now is the only time we have access to the PID of
		   the fully daemonized process. */
		daemon_write_pid(pid, path);
		_exit(0);
	}
}

static void daemon_write_pid(pid_t pid, const char *path)
{
	FILE *io;

	io = fopen(path, "w");
	if (!io) {
		ERROR("unable to open PID file %s for writing: %s", path, strerror(errno));
		exit(2);
	}
	fprintf(io, "%lu\n", (unsigned long)pid);
	fclose(io);
}

static void daemon_settle(void)
{
	umask(0777); /* reset the file umask */
	if (chdir("/") < 0) {
		ERROR("unable to chdir to /: %s", strerror(errno));
		exit(2);
	}

	/* redirect standard fds */
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
}


/**********************************************************/

int server_init(server *s)
{
	assert(s);

	struct sigaction sig;

	if (manifest) {
		ERROR("server module already initialized");
		return -1;
	}

	pthread_mutex_lock(&manifest_mutex);
		manifest_file = s->manifest_file;
		manifest = parse_file(manifest_file);
	pthread_mutex_unlock(&manifest_mutex);

	protocol_ssl_init();
	s->ssl_ctx = protocol_ssl_default_context(s->ca_cert_file,
	                                          s->cert_file,
	                                          s->key_file);

	if (!s->ssl_ctx) {
		return -1;
	}

	/* sig handlers */
	INFO("setting up signal handlers");
	sig.sa_sigaction = server_sighup;
	sig.sa_flags = SA_SIGINFO;
	sigemptyset(&sig.sa_mask);
	if (sigaction(SIGHUP, &sig, NULL) != 0) {
		ERROR("Unable to set signal handlers: %s", strerror(errno));
		/* FIXME: cleanup */
		return -1;
	}

	/* daemonize, if necessary */
	if (s->daemonize == SERVER_OPT_TRUE) {
		INFO("daemonizing");
		daemonize(s->lock_file, s->pid_file);
		log_init("policyd");
	} else {
		INFO("running in foreground");
	}

	log_level(s->log_level);

	/* bind socket */
	INFO("binding SSL socket on port %s", s->port);
	s->listener = BIO_new_accept(strdup(s->port));
	if (!s->listener || BIO_do_accept(s->listener) <= 0) {
		return -1;
	}

	return 0;
}

int server_deinit(server *s)
{
	SSL_CTX_free(s->ssl_ctx);
	return 0;
}

int server_loop(server *s)
{
	assert(s);
	worker *w;

	for (;;) {
		w = worker_spawn(s);
		if (!w) {
			WARNING("Failed to spawn worker for inbound connection");
			continue;
		}
		THREAD_CREATE(w->tid, server_worker_thread, w);
	}

	return -1;
}

void* server_worker_thread(void *arg)
{
	worker *w = (worker*)arg;

	pthread_detach(pthread_self());

	if (worker_prep(w) != 0) {
		worker_die(w);
		return NULL;
	}

	worker_dispatch(w);
	worker_die(w);

	INFO("SSL connection closed");
	return NULL;
}

void* server_manager_thread(void *arg)
{
	struct manifest *new_manifest;

	new_manifest = parse_file(manifest_file);
	if (new_manifest) {
		INFO("Updating server manifest");
		pthread_mutex_lock(&manifest_mutex);
		manifest = new_manifest;
		pthread_mutex_unlock(&manifest_mutex);
	} else {
		WARNING("Unable to parse manifest; skipping");
	}

	return NULL;
}
