/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "clockwork.h"

#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <linux/limits.h>

#include "cert.h"
#include "spec/parser.h"
#include "server.h"
#include "resources.h"
#include "db.h"

/**************************************************************/

struct worker {
	BIO *socket;
	SSL *ssl;
	THREAD_TYPE tid;

	char *peer;

	const char *ca_cert_file;
	const char *key_file;
	const char *requests_dir;
	const char *certs_dir;
	const char *db_file;
	const char *cache_dir;

	struct hash   *facts;
	struct policy *policy;
	struct stree  *pnode;

	struct session session;

	unsigned short peer_verified;
	unsigned short autosign;
};

/**************************************************************/

static const char      *manifest_file = NULL;
static struct manifest *manifest = NULL;
static pthread_mutex_t  manifest_mutex = PTHREAD_MUTEX_INITIALIZER;

/**************************************************************/

static struct server* policyd_options(int argc, char **argv);
static void show_version(void);
static void show_help(void);

static struct worker* spawn(struct server *s);
static int worker_prep(struct worker *w);
static int verify_peer(struct worker *w);
static int send_file(struct worker *w, const char *path);
static int send_template(struct worker *w, const char *path);

static int handle_HELLO(struct worker *w);
static int handle_GET_CERT(struct worker *w);
static int handle_FACTS(struct worker *w);
static int handle_FILE(struct worker *w);
static int handle_REPORT(struct worker *w);
static int handle_unknown(struct worker *w);

static void sighup_handler(int, siginfo_t*, void*);
static void sigterm_handler(int, siginfo_t*, void*);

static void daemonize(const char *lock_file, const char *pid_file);

static void show_config(struct server *s);

static int check_config(struct server *s);
static int server_init_ssl(struct server *s);
static int server_init(struct server *s);

static int save_facts(struct worker *w);

/* Thread execution functions */
static void* worker_thread(void *arg);
static void* signal_thread(void *arg);

/**************************************************************/

int main(int argc, char **argv)
{
	struct server *s;
	struct worker *w;

	s = policyd_options(argc, argv);
	if (server_options(s) != 0) {
		fprintf(stderr, "Unable to process server options");
		exit(2);
	}

	s->log_level = log_set(s->log_level);
	INFO("Log level is %s (%u)", log_level_name(s->log_level), s->log_level);

	if (s->show_config == SERVER_OPT_TRUE) {
		INFO("Dumping policyd configuration");
		show_config(s);
		exit(0);
	}

	if (check_config(s) != 0) {
		exit(1);
	}

	pthread_mutex_lock(&manifest_mutex);
		manifest_file = s->manifest_file;
		manifest = parse_file(manifest_file);
	pthread_mutex_unlock(&manifest_mutex);

	if (!manifest) {
		exit(1);
	}

	/* bail if we are just testing the config */
	if (s->test_config == SERVER_OPT_TRUE) {
		printf("policyd config OK\n");
		exit(0);
	}

	INFO("policyd starting up");
	if (server_init(s) != 0) {
		CRITICAL("Failed to initialize policyd server thread");
		exit(2);
	}

	// Set up the signal processing thread
	THREAD_TYPE tid;
	THREAD_CREATE(tid, signal_thread, NULL);

	sigset_t blocked_sigs;
	sigemptyset(&blocked_sigs);
	sigaddset(&blocked_sigs, SIGHUP);
	sigaddset(&blocked_sigs, SIGINT);
	sigaddset(&blocked_sigs, SIGUSR1);
	sigaddset(&blocked_sigs, SIGUSR2);
	sigaddset(&blocked_sigs, SIGALRM);

	DEBUG("entering server_loop");
	for (;;) {
		w = spawn(s);
		if (!w) {
			WARNING("Failed to spawn worker for inbound connection");
			continue;
		}
		THREAD_CREATE(w->tid, worker_thread, w);
	}

	SSL_CTX_free(s->ssl_ctx);
	INFO("policyd shutting down");
	return 0;
}

/**************************************************************/

static struct server* policyd_options(int argc, char **argv)
{
	struct server *s;

	const char *short_opts = "h?FDvqQc:sl:tV";
	struct option long_opts[] = {
		{ "help",         no_argument,       NULL, 'h' },
		{ "no-daemonize", no_argument,       NULL, 'F' },
		{ "foreground",   no_argument,       NULL, 'F' },
		{ "debug",        no_argument,       NULL, 'D' },
		{ "silent",       no_argument,       NULL, 'Q' },
		{ "config",       required_argument, NULL, 'c' },
		{ "show-config",  no_argument,       NULL, 's' },
		{ "listen",       required_argument, NULL, 'l' },
		{ "test",         no_argument,       NULL, 't' },
		{ "version",      no_argument,       NULL, 'V' },
		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;

	s = xmalloc(sizeof(struct server));

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1 ) {
		switch (opt) {
		case 'h':
		case '?':
			show_help();
			exit(0);
		case 'V':
			show_version();
			exit(0);
		case 'F':
			s->daemonize = SERVER_OPT_FALSE;
			break;
		case 'D':
			s->daemonize = SERVER_OPT_FALSE;
			s->debug = 1;
			break;
		case 'v':
			s->log_level++;
			break;
		case 'q':
			s->log_level--;
			break;
		case 'Q':
			s->log_level = 0;
			break;
		case 'c':
			free(s->config_file);
			s->config_file = strdup(optarg);
			break;
		case 's':
			s->show_config = SERVER_OPT_TRUE;
			break;
		case 'l':
			free(s->listen);
			s->listen = strdup(optarg);
			break;
		case 't':
			s->test_config = SERVER_OPT_TRUE;
			break;
		}
	}

	return s;
}

static void show_version(void)
{
	printf("policyd (Clockwork) " VERSION "\n"
	       "Copyright 2011-2013 James Hunt\n");
}

static void show_help(void)
{
	printf("USAGE: policyd [OPTIONS]\n"
	       "\n"
	       "  -h, --help            Show this helpful message.\n"
	       "                        (for more in-depth help, check the man pages.)\n"
	       "\n"
	       "  -V, --version         Print version and copyright information.\n"
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
	       "                        only CRITICAL and ERROR messages are logged, and all\n"
	       "                        others are discarded.  See -q and -v.\n"
	       "\n"
	       "  -c, --config          Specify the path to an alternate configuration file.\n"
	       "\n"
	       "  -t, --test            Run server-side diagnostics against the configuration.\n"
	       "                        This is designed to catch errors like a missing cache\n"
	       "                        directory, bad certs_dir perms, etc.  Use it.  Love it.\n"
	       "\n"
	       "  -s, --show-config     Dump the policyd configuration values to standard\n"
	       "                        output, and then exit.\n"
	       "\n"
	       "  -l, --listen          Override the network interface and port number that\n"
	       "                        policyd should bind to and listen on.\n"
	       "\n");
}

static struct worker* spawn(struct server *s)
{
	struct worker *w;
	DEBUG("Spawning new worker");

unintr:
	errno = 0;
	if (BIO_do_accept(s->listener) <= 0) {
		if (errno == EINTR) {
			DEBUG("Catching interruption due to SIGHUP(?)");
			goto unintr;
		}
		WARNING("Couldn't accept inbound connection");
		protocol_ssl_backtrace();
		return NULL;
	}

	w = xmalloc(sizeof(struct worker));
	w->facts = hash_new();
	w->peer_verified = 0;

	/* constant references to server config */
	w->requests_dir = s->requests_dir;
	w->certs_dir    = s->certs_dir;
	w->db_file      = s->db_file;
	w->cache_dir    = s->cache_dir;
	w->ca_cert_file = s->ca_cert_file;
	w->key_file     = s->key_file;
	w->autosign     = s->autosign;

	w->socket = BIO_pop(s->listener);
	if (!(w->ssl = SSL_new(s->ssl_ctx))) {
		WARNING("Couldn't create SSL handle for worker thread");
		protocol_ssl_backtrace();

		hash_free_all(w->facts);
		free(w);
		return NULL;
	}

	SSL_set_bio(w->ssl, w->socket, w->socket);
	return w;
}

static int worker_prep(struct worker *w)
{
	assert(w); // LCOV_EXCL_LINE

	sigset_t blocked_signals;

	DEBUG("Preparing new worker");

	/* block SIGPIPE for clients that close early. */
	sigemptyset(&blocked_signals);
	sigaddset(&blocked_signals, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &blocked_signals, NULL);

	if (SSL_accept(w->ssl) <= 0) {
		DEBUG("SSL_accept returned non-zero");
		ERROR("Unable to accept inbound SSL connection");
		protocol_ssl_backtrace();
		return -1;
	}

	if (verify_peer(w) != 0) {
		DEBUG("verify_peer returned non-zero");
		ERROR("Unable to verify peer");
		return -1;
	}

	protocol_session_init(&w->session, w->ssl);

	pthread_mutex_lock(&manifest_mutex);
		w->pnode = hash_get(manifest->hosts, w->peer);
		if (!w->pnode) {
			w->pnode = manifest->fallback;
		}
	pthread_mutex_unlock(&manifest_mutex);

	w->policy = NULL;
	return 0;
}

static int handle_HELLO(struct worker *w)
{
	if (w->peer_verified) {
		pdu_send_HELLO(&w->session);
	} else {
		pdu_send_ERROR(&w->session, 401, "Peer Certificate Required");
	}
	return 1;
}

static int handle_GET_CERT(struct worker *w)
{
	char *csr_file, *cert_file;
	EVP_PKEY *key;
	X509_REQ *csr = X509_REQ_new();
	X509 *cert = NULL, *ca_cert;

	if (pdu_decode_GET_CERT(RECV_PDU(&w->session), &csr) != 0) {
		CRITICAL("Unable to decode GET_CERT PDU");
		return 0;
	}

	csr_file  = string("%s/%s.csr", w->requests_dir, w->peer);
	cert_file = string("%s/%s.pem", w->certs_dir, w->peer);
	if (csr) {
		unlink(cert_file);

		if (w->autosign) {
			INFO("Auto-signing inbound Certificate Request");

			key = cert_retrieve_key(w->key_file);
			if (!key) {
				CRITICAL("Failed to retrieve CA private key");
				return 0;
			}

			ca_cert = cert_retrieve_certificate(w->ca_cert_file);
			cert = cert_sign_request(csr, ca_cert, key, 365*10);
			if (!cert) {
				CRITICAL("Failed to auto-sign certificate");
				return 0;
			}

			if (cert_store_certificate(cert, cert_file) != 0) {
				CRITICAL("Failed to store signed client certificate");
				return 0;
			}

		} else {
			INFO("Storing inbound Certificate Request");
			cert_store_request(csr, csr_file);
		}
	} else {
		cert = cert_retrieve_certificate(cert_file);
	}
	free(csr_file);
	free(cert_file);

	if (pdu_send_SEND_CERT(&w->session, cert) < 0) { return 0; }
	return 1;
}

static int handle_FACTS(struct worker *w)
{
	if (!w->peer_verified) {
		WARNING("Unverified peer tried to FACTS");
		pdu_send_ERROR(&w->session, 401, "Peer Certificate Required");
		return 1;
	}

	if (pdu_decode_FACTS(RECV_PDU(&w->session), w->facts) != 0) {
		CRITICAL("Unable to decode FACTS");
		return 0;
	}

	if (save_facts(w) != 0) {
		WARNING("Failed to save facts to a local file");
	}

	if (!w->policy && w->pnode) {
		w->policy = policy_generate(w->pnode, w->facts);
	}

	if (!w->policy) {
		CRITICAL("Unable to generate policy for host %s", w->peer);
		return 0;
	}

	if (pdu_send_POLICY(&w->session, w->policy) < 0) { return 0; }

	return 1;
}

static int handle_FILE(struct worker *w)
{
	char hex[SHA1_HEXLEN] = {0};
	struct SHA1 checksum;
	struct resource *res;
	struct res_file *file, *match;

	if (!w->peer_verified) {
		WARNING("Unverified peer tried to FILE");
		pdu_send_ERROR(&w->session, 401, "Peer Certificate Required");
		return 1;
	}

	if (RECV_PDU(&w->session)->len != SHA1_HEXLEN - 1) {
		pdu_send_ERROR(&w->session, 400, "Malformed FILE request");
		return 1;
	}

	DEBUG("Received FILE request for %s", RECV_PDU(&w->session)->data);

	memcpy(hex, RECV_PDU(&w->session)->data, SHA1_HEXLEN);
	sha1_init(&checksum, hex);

	if (checksum.hex[0] == '\0') {
		pdu_send_ERROR(&w->session, 400, "Malformed FILE request");
		return 1;
	}

	/* Search for the res_file in the policy */
	if (!w->policy) {
		WARNING("FILE before FACTS");
		pdu_send_ERROR(&w->session, 405, "Called FILE before FACTS");
		return 0;
	}

	file = NULL;
	for_each_resource(res, w->policy) {
		if (res->type == RES_FILE) {
			match = (struct res_file*)(res->resource);
			DEBUG("compare %s == %s", match->rf_rsha1.hex, checksum.hex);
			if (sha1_cmp(&match->rf_rsha1, &checksum) == 0) {
				file = match;
				break;
			}
		}
	}

	if (file) {
		DEBUG("Found file (%s):\n", file->key);
		DEBUG("  checksum:    %s\n", checksum.hex);
		DEBUG("  rf_rpath:    %s\n", file->rf_rpath);
		DEBUG("  rf_template: %s\n", file->rf_template);

		if (file->rf_rpath) {
			INFO("Matched %s to %s", checksum.hex, file->rf_rpath);
			if (send_file(w, file->rf_rpath) != 0) {
				DEBUG("Unable to send file");
			}
		} else if (file->rf_template) {
			INFO("Matched %s to template %s", checksum.hex, file->rf_template);
			if (send_template(w, file->rf_template) != 0) {
				DEBUG("Unable to send template contents");
			}
		} else {
			DEBUG("Unknown file source type (not rpath / not template)");
			INFO("File Not Found: %s", checksum.hex);
			pdu_send_ERROR(&w->session, 404, "File Not Found");
		}
	} else {
		INFO("File Not Found: %s", checksum.hex);
		pdu_send_ERROR(&w->session, 404, "File Not Found");
	}

	return 1;
}

static int handle_REPORT(struct worker *w)
{
	struct job *job;
	rowid host_id;
	struct db *db = NULL;

	if (!w->peer_verified) {
		WARNING("Unverified peer tried to REPORT");
		pdu_send_ERROR(&w->session, 401, "Peer Certificate Required");
		return 1;
	}

	if (pdu_decode_REPORT(RECV_PDU(&w->session), &job) != 0) {
		CRITICAL("Unable to decode REPORT");
		return 0;
	}

	db = db_open(MASTERDB, w->db_file);
	if (!db) {
		CRITICAL("Unable to open reporting DB");
		goto failed;
	}

	host_id = masterdb_host(db, w->peer);
	if (host_id == NULL_ROWID) {
		CRITICAL("Failed to find or create host record for '%s'", w->peer);
		goto failed;
	}

	if (masterdb_store_report(db, host_id, job) != 0) {
		CRITICAL("Unable to save report in reporting DB");
		goto failed;
	}
	INFO("saved client report in reporting database");
	db_close(db);
	job_free(job);

	if (pdu_send_BYE(&w->session) < 0) { return 0; }

	return 1;

failed:
	db_close(db);
	job_free(job);
	return 0;
}

static int handle_unknown(struct worker *w)
{
	char *message;
	enum proto_op op = RECV_PDU(&w->session)->op;

	message = string("Protocol Error: Unrecognized PDU op %s(%u)", protocol_op_name(op), op);
	WARNING("%s", message);
	pdu_send_ERROR(&w->session, 405, message);
	free(message);

	return 1;
}

static int verify_peer(struct worker *w)
{
	assert(w); // LCOV_EXCL_LINE

	long err;
	int sock;
	char addr[256];

	sock = SSL_get_fd(w->ssl);
	if (protocol_reverse_lookup_verify(sock, addr, 256) != 0) {
		ERROR("FCrDNS lookup (ipv4) failed: %s", strerror(errno));
		return -1;
	}
	w->peer = strdup(addr);
	INFO("Connection on socket %u from '%s'", sock, w->peer);

	w->peer_verified = 0;

	if ((err = protocol_ssl_verify_peer(w->ssl, addr)) != X509_V_OK) {
		if (err != X509_V_ERR_APPLICATION_VERIFICATION) {
			ERROR("SSL: problem with peer certificate: %s", X509_verify_cert_error_string(err));
			protocol_ssl_backtrace();
		}
	} else {
		w->peer_verified = 1;
	}

	return 0;
}

static int send_file(struct worker *w, const char *path)
{
	int fd = open(path, O_RDONLY);
	int n;
	while ((n = pdu_send_DATA(&w->session, fd, NULL)) > 0)
		;
	close(fd);

	return n;
}

static int send_template(struct worker *w, const char *path)
{
	struct template *t;
	char *data, *p;
	int n;

	t = template_create(path, w->facts);
	if (!t) {
		pdu_send_ERROR(&w->session, 510, "Missing");
		return 1;
	}

	p = data = template_render(t);

	while ((n = pdu_send_DATA(&w->session, -1, p)) > 0) {
		p += n;
	}

	free(t); free(data);
	return n;
}

static void sighup_handler(int signum, siginfo_t *info, void *udata)
{
	struct manifest *new_manifest;

	new_manifest = parse_file(manifest_file);
	if (new_manifest) {
		INFO("Updating server manifest");
		pthread_mutex_lock(&manifest_mutex);
		manifest_free(manifest);
		manifest = new_manifest;
		pthread_mutex_unlock(&manifest_mutex);
	} else {
		WARNING("Unable to parse manifest; skipping");
	}
}

static void sigterm_handler(int signum, siginfo_t *info, void *udata)
{
	INFO("Caught SIGTERM/SIGINT; requesting clean shutdown");
	pthread_mutex_lock(&manifest_mutex);
	manifest_free(manifest);
	exit(1);
}

/**
  Daemonize.

  Daemonizes the current process, by forking into the background,
  closing stdin, stdout and stderr, and detaching from the
  controlling terminal.

  $lock_file contains the path to use as a lock, preventing multiple
  daemon instances from running concurrently.

  $pid_file contains the path to store the daemon's process ID in,
  for out-of-band process management (i.e. init scripts).


  On success, control is returned to the caller.  The parent process
  exits with a status code of 0.

  On failure, this call will not return, and the current process exits
  with a non-zero status code.
 */
static void daemonize(const char *lock_file, const char *pid_file)
{
	pid_t pid, sessid;
	int lock_fd;
	FILE *pid_io;

	/* are we already a child of init? */
	if (getppid() == 1) { return; }

	/* first fork */
	pid = fork();
	if (pid < 0) {
		ERROR("unable to fork: %s", strerror(errno));
		exit(2);
	}
	if (pid > 0) { /* parent process */
		_exit(0);
	}

	/* second fork */
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
		/* write pid to file
		     Now is the only time we have access to the PID of
		     the fully daemonized process. */
		pid_io = fopen(pid_file, "w");
		if (!pid_io) {
			ERROR("Failed to open PID file %s for writing: %s", pid_file, strerror(errno));
			exit(2);
		}
		fprintf(pid_io, "%lu\n", (unsigned long)pid);
		fclose(pid_io);
		_exit(0);
	}

	/* acquire lock */
	if (!(lock_file && lock_file[0])) {
		ERROR("NULL or empty lock file path given");
		exit(2);
	}

	lock_fd = open(lock_file, O_CREAT | O_RDWR, 0640);
	if (lock_fd < 0) {
		ERROR("Failed to open lock file %s: %s", lock_file, strerror(errno));
		exit(2);
	}

	if (lockf(lock_fd, F_TLOCK, 0) < 0) {
		ERROR("Failed to lock %s; daemon already running?", lock_file);
		exit(2);
	}

	/* settle */
	umask(0777); /* reset the file umask */
	if (chdir("/") < 0) {
		ERROR("unable to chdir to /: %s", strerror(errno));
		exit(2);
	}

	/* redirect standard fds */
	if (!freopen("/dev/null", "r", stdin)
	 || !freopen("/dev/null", "w", stdout)
	 || !freopen("/dev/null", "w", stderr)) {
		NOTICE("Failed to redirect IO streams to /dev/null");
	}
}

static void show_config(struct server *s)
{
	printf("# Clockwork Policy Master Configuration\n");
	printf("\n");
	printf("ca_cert_file  = \"%s\"\n", s->ca_cert_file);
	printf("cert_file     = \"%s\"\n", s->cert_file);
	printf("key_file      = \"%s\"\n", s->key_file);
	printf("crl_file      = \"%s\"\n", s->crl_file);
	printf("\n");
	printf("requests_dir  = \"%s\"\n", s->requests_dir);
	printf("certs_dir     = \"%s\"\n", s->certs_dir);
	printf("cache_dir     = \"%s\"\n", s->cache_dir);
	printf("\n");
	printf("lock_file     = \"%s\"\n", s->lock_file);
	printf("pid_file      = \"%s\"\n", s->pid_file);
	printf("\n");
	printf("manifest_file = \"%s\"\n", s->manifest_file);
	printf("db_file       = \"%s\"\n", s->db_file);
	printf("\n");
	printf("listen        = %s\n", s->listen);
	printf("\n");
	printf("log_level     = %s\n", log_level_name(s->log_level));
	printf("\n");
}

static int check_dir_writable(const char *dir)
{
	struct stat st;
	DEBUG("%s: stat %s", __func__, dir);
	if (stat(dir, &st) != 0) {
		DEBUG("%s:   failed; errno = %d", __func__, errno);
		return errno;
	}

	DEBUG("%s: generate tmp filename in %s", __func__, dir);
	char tmp[PATH_MAX];
	if (snprintf(tmp, sizeof(tmp), "%s/.policyd.check", dir) <= 0) {
		DEBUG("%s:   failed; errno = %d", __func__, errno);
		return errno;
	}

	DEBUG("%s: creat %s", __func__, tmp);
	int fd = creat(tmp, O_WRONLY);
	if (fd < 0) {
		DEBUG("%s:   failed; errno = %d", __func__, errno);
		return errno;
	}
	close(fd);

	DEBUG("%s: unlink %s", __func__, tmp);
	unlink(tmp);
	return 0;
}

static int check_file_writable(const char *path)
{
	struct stat st;
	DEBUG("%s: stat %s", __func__, path);
	if (stat(path, &st) != 0) {
		DEBUG("%s:   failed; errno = %d", __func__, errno);
		return errno;
	}

	DEBUG("%s: open %s, O_APPEND", __func__, path);
	int fd = open(path, O_APPEND);
	if (fd < 0) {
		DEBUG("%s:   failed; errno = %d", __func__, errno);
		return errno;
	}
	close(fd);

	return 0;
}

static int check_file_readable(const char *path)
{
	struct stat st;
	DEBUG("%s: stat %s", __func__, path);
	if (stat(path, &st) != 0) {
		DEBUG("%s:   failed; errno = %d", __func__, errno);
		return errno;
	}

	DEBUG("%s: open %s, O_RDONLY", __func__, path);
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		DEBUG("%s:   failed; errno = %d", __func__, errno);
		return errno;
	}
	close(fd);

	return 0;
}

static int check_config(struct server *s)
{
	char *copy, *dir;
	int rc = 0;
	int err = 0;

	if ((err = check_file_readable(s->manifest_file)) != 0) {
		fprintf(stderr, "manifest_file (%s) : %s\n", s->manifest_file, strerror(err));
		rc = 1;
	}

	/* pid_file directory exists && writable */
	dir = dirname(copy = strdup(s->pid_file));
	if ((err = check_dir_writable(dir)) != 0) {
		fprintf(stderr, "pid_file directory (%s) : %s\n", dir, strerror(err));
		rc = 1;
	}
	free(copy);

	/* lock_file directory exists && writable */
	dir = dirname(copy = strdup(s->lock_file));
	if ((err = check_dir_writable(dir)) != 0) {
		fprintf(stderr, "lock_file directory (%s) : %s\n", dir, strerror(err));
		rc = 1;
	}
	free(copy);

	/* db_file (exists && writable) || dir_writable) */
	if ((err = check_file_writable(s->db_file)) != 0) {
		if (err == ENOENT) {
			dir = dirname(copy = strdup(s->db_file));
			if ((err = check_dir_writable(dir)) != 0) {
				fprintf(stderr, "db_file directory (%s) : %s\n", dir, strerror(err));
				rc = 1;
			}
			free(copy);
		} else {
			fprintf(stderr, "db_file (%s) : %s\n", s->db_file, strerror(err));
			rc = 1;
		}
	}

	/* cache_dir exists && writable */
	if ((err = check_dir_writable(s->cache_dir)) != 0) {
		fprintf(stderr, "cache_dir (%s) : %s\n", s->cache_dir, strerror(err));
		rc = 1;
	}

	/* ca_cert_file exists && readable */
	if ((err = check_file_readable(s->ca_cert_file)) != 0) {
		fprintf(stderr, "ca_cert_file (%s) : %s\n", s->ca_cert_file, strerror(err));
		rc = 1;
	}

	/* cert_file exists && readable */
	if ((err = check_file_readable(s->cert_file)) != 0) {
		fprintf(stderr, "cert_file (%s) : %s\n", s->cert_file, strerror(err));
		rc = 1;
	}

	/* key_file exists && readable */
	if ((err = check_file_readable(s->key_file)) != 0) {
		fprintf(stderr, "key_file (%s) : %s\n", s->key_file, strerror(err));
		rc = 1;
	}

	/* requests_dir exists && writable */
	if ((err = check_dir_writable(s->requests_dir)) != 0) {
		fprintf(stderr, "requests_dir (%s) : %s\n", s->requests_dir, strerror(err));
		rc = 1;
	}

	/* certs_dir exists && writable */
	if ((err = check_dir_writable(s->certs_dir)) != 0) {
		fprintf(stderr, "certs_dir (%s) : %s\n", s->certs_dir, strerror(err));
		rc = 1;
	}

	return rc;
}

static int server_init(struct server *s)
{
	assert(s); // LCOV_EXCL_LINE

	if (server_init_ssl(s) != 0) { return -1; }

	/* daemonize, if necessary */
	if (s->daemonize == SERVER_OPT_TRUE) {
		INFO("daemonizing");
		daemonize(s->lock_file, s->pid_file);
		log_init("policyd");
	} else {
		INFO("running in foreground");
	}

	/* bind socket */
	INFO("binding SSL socket on %s", s->listen);
	s->listener = BIO_new_accept(s->listen);
	if (!s->listener || BIO_do_accept(s->listener) <= 0) {
		return -1;
	}

	return 0;
}

static int save_facts(struct worker *w)
{
	char *path;
	FILE *out;

	path = string("%s/facts/%s.facts", w->cache_dir, w->peer);
	out = fopen(path, "w");
	free(path);
	if (!out) {
		DEBUG("save_facts: failed to fopen() facts file");
		return -2;
	}

	if (fact_write(out, w->facts) != 0) {
		fclose(out);
		DEBUG("save_facts: fact_write call failed");
		return -3;
	}

	fclose(out);
	return 0;
}

static void* worker_thread(void *arg)
{
	struct worker *w = (struct worker*)arg;
	int done = 0;

	pthread_detach(pthread_self());
	if (worker_prep(w) != 0) { goto die; }

	/* dispatch */
	while (!done) {
		if (pdu_receive(&w->session) < 0) {
			WARNING("Remote end hung up unexpectedly");
			break;
		}

		switch (RECV_PDU(&w->session)->op) {

		case PROTOCOL_OP_HELLO:
			if (!handle_HELLO(w)) { done = 1; }
			break;

		case PROTOCOL_OP_BYE:
			done = 1;
			break;

		case PROTOCOL_OP_GET_CERT:
			if (!handle_GET_CERT(w)) { done = 1; }
			break;

		case PROTOCOL_OP_FACTS:
			if (!handle_FACTS(w)) { done = 1; }
			break;

		case PROTOCOL_OP_FILE:
			if (!handle_FILE(w)) { done = 1; }
			break;

		case PROTOCOL_OP_REPORT:
			if (!handle_REPORT(w)) { done = 1; }
			break;

		default:
			handle_unknown(w);
			done = 1;
			break;
		}
	}

	INFO("SSL connection closed");

die:
	SSL_shutdown(w->ssl);
	SSL_free(w->ssl);
	ERR_remove_state(0);

	free(w->peer);
	hash_free_all(w->facts);
	policy_free_all(w->policy);
	protocol_session_deinit(&w->session);
	free(w);

	return NULL;
}

static void* signal_thread(void *arg)
{
	struct sigaction sig;

	/* SIGHUP: reload policy configuration */
	INFO("setting up signal handlers");
	sig.sa_sigaction = sighup_handler;
	sig.sa_flags = SA_SIGINFO;
	sigemptyset(&sig.sa_mask);
	if (sigaction(SIGHUP, &sig, NULL) != 0) {
		ERROR("Unable to set SIGHUP handler: %s", strerror(errno));
		exit(2);
	}

	/* SIGTERM: clean shutdown */
	sig.sa_sigaction = sigterm_handler;
	sig.sa_flags = SA_SIGINFO;
	sigemptyset(&sig.sa_mask);
	if (sigaction(SIGTERM, &sig, NULL) != 0) {
		ERROR("Unable to set SIGTERM handler: %s", strerror(errno));
		exit(2);
	}

	/* SIGINTO: clean shutdown */
	sig.sa_sigaction = sigterm_handler;
	sig.sa_flags = SA_SIGINFO;
	sigemptyset(&sig.sa_mask);
	if (sigaction(SIGINT, &sig, NULL) != 0) {
		ERROR("Unable to set SIGINT handler: %s", strerror(errno));
		exit(2);
	}

	for (;;)
		;
}

static int server_init_ssl(struct server *s)
{
	assert(s->ca_cert_file); // LCOV_EXCL_LINE
	assert(s->cert_file); // LCOV_EXCL_LINE
	assert(s->key_file); // LCOV_EXCL_LINE

	X509_STORE *store;
	X509_CRL *crl;

	protocol_ssl_init();

	INFO("Setting up server SSL context");
	if (!(s->ssl_ctx = SSL_CTX_new(TLSv1_method()))) {
		ERROR("Failed to set up new TLSv1 SSL context");
		protocol_ssl_backtrace();
		return -1;
	}

	DEBUG(" - Loading CA certificate chain from %s", s->ca_cert_file);
	if (!SSL_CTX_load_verify_locations(s->ssl_ctx, s->ca_cert_file, NULL)) {
		ERROR("Failed to load CA certificate chain (%s)", s->ca_cert_file);
		goto error;
	}

	DEBUG(" - Loading certificate from %s", s->cert_file);
	if (!SSL_CTX_use_certificate_file(s->ssl_ctx, s->cert_file, SSL_FILETYPE_PEM)) {
		ERROR("Failed to load certificate from file (%s)", s->cert_file);
		goto error;
	}

	DEBUG(" - Loading private key from %s", s->key_file);
	if (!SSL_CTX_use_PrivateKey_file(s->ssl_ctx, s->key_file, SSL_FILETYPE_PEM)) {
		ERROR("Failed to load private key (%s)", s->key_file);
		goto error;
	}

	DEBUG(" - Setting peer verification flags");
	SSL_CTX_set_verify(s->ssl_ctx, SSL_VERIFY_PEER, NULL);
	SSL_CTX_set_verify_depth(s->ssl_ctx, 4);

	crl = cert_retrieve_crl(s->crl_file);
	if (crl) {
		DEBUG(" - Loading certificate revocation list from %s", s->crl_file);
		store = SSL_CTX_get_cert_store(s->ssl_ctx);
		X509_STORE_add_crl(store, crl);

		s->ssl_ctx->param = X509_VERIFY_PARAM_new();
		X509_VERIFY_PARAM_set_flags(s->ssl_ctx->param, X509_V_FLAG_CRL_CHECK);
	}

	return 0;

error:
	SSL_CTX_free(s->ssl_ctx);
	s->ssl_ctx = NULL;
	protocol_ssl_backtrace();
	return -1;
}

