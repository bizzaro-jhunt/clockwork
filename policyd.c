#include "clockwork.h"

#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cert.h"
#include "spec/parser.h"
#include "server.h"
#include "resources.h"
#include "db.h"

/**************************************************************/

/** @cond false */
typedef struct {
	BIO *socket;
	SSL *ssl;
	THREAD_TYPE tid;

	char *peer;

	const char *requests_dir;
	const char *certs_dir;
	const char *db_file;
	const char *cache_dir;

	struct hash   *facts;
	struct policy *policy;
	struct stree  *pnode;

	protocol_session session;

	unsigned short peer_verified;
} worker;
/** @endcond */

/**************************************************************/

static const char      *manifest_file = NULL;
static struct manifest *manifest = NULL;
static pthread_mutex_t  manifest_mutex = PTHREAD_MUTEX_INITIALIZER;

/**************************************************************/

static server* policyd_options(int argc, char **argv);
static void show_help(void);

static worker* spawn(server *s);
static int worker_prep(worker *w);
static int verify_peer(worker *w);
static int send_file(worker *w, const char *path);
static int send_template(worker *w, const char *path);

static int handle_HELLO(worker *w);
static int handle_GET_CERT(worker *w);
static int handle_FACTS(worker *w);
static int handle_FILE(worker *w);
static int handle_REPORT(worker *w);
static int handle_unknown(worker *w);

static void sighup_handler(int, siginfo_t*, void*);

static void daemonize(const char *lock_file, const char *pid_file);

static void show_config(server *s);

static int server_init_ssl(server *s);
static int server_init(server *s);

static int save_facts(worker *w);

/* Thread execution functions */
static void* worker_thread(void *arg);
static void* manager_thread(void *arg);

/**************************************************************/

int main(int argc, char **argv)
{
	server *s;
	worker *w;

	s = policyd_options(argc, argv);
	if (server_options(s) != 0) {
		fprintf(stderr, "Unable to process server options");
		exit(2);
	}

	if (s->show_config == SERVER_OPT_TRUE) {
		INFO("Dumping policyd configuration");
		show_config(s);
		exit(0);
	}

	INFO("policyd starting up");
	if (server_init(s) != 0) {
		CRITICAL("Failed to initialize policyd server thread");
		exit(2);
	}
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
	return 0;
}

/**************************************************************/

static server* policyd_options(int argc, char **argv)
{
	server *s;

	const char *short_opts = "h?FDvqQc:p:";
	struct option long_opts[] = {
		{ "help",         no_argument,       NULL, 'h' },
		{ "no-daemonize", no_argument,       NULL, 'F' },
		{ "foreground",   no_argument,       NULL, 'F' },
		{ "debug",        no_argument,       NULL, 'D' },
		{ "silent",       no_argument,       NULL, 'Q' },
		{ "config",       required_argument, NULL, 'c' },
		{ "show-config",  no_argument,       NULL, 's' },
		{ "port",         required_argument, NULL, 'p' },
		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;

	s = xmalloc(sizeof(server));

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1 ) {
		switch (opt) {
		case 'h':
		case '?':
			show_help();
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
		case 'p':
			free(s->port);
			s->port = strdup(optarg);
			break;
		}
	}

	return s;
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
	       "                        only CRITICAL and ERROR messages are logged, and all\n"
	       "                        others are discarded.  See -q and -v.\n"
	       "\n"
	       "  -c, --config          Specify the path to an alternate configuration file.\n"
	       "\n"
	       "  -s, --show-config     Dump the policyd configuration values to standard\n"
	       "                        output, and then exit.\n"
	       "\n"
	       "  -p, --port            Override the TCP port number that policyd should\n"
	       "                        bind to and listen on.\n"
	       "\n");
}

static worker* spawn(server *s)
{
	worker *w;
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

	w = xmalloc(sizeof(worker));
	w->facts = hash_new();
	w->peer_verified = 0;

	/* constant references to server config */
	w->requests_dir = s->requests_dir;
	w->certs_dir    = s->certs_dir;
	w->db_file      = s->db_file;
	w->cache_dir    = s->cache_dir;

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
	pthread_mutex_unlock(&manifest_mutex);

	w->policy = NULL;
	return 0;
}

static int handle_HELLO(worker *w)
{
	if (w->peer_verified) {
		pdu_send_HELLO(&w->session);
	} else {
		pdu_send_ERROR(&w->session, 401, "Peer Certificate Required");
	}
	return 1;
}

static int handle_GET_CERT(worker *w)
{
	char *csr_file, *cert_file;
	X509_REQ *csr = X509_REQ_new();
	X509 *cert = NULL;

	if (pdu_decode_GET_CERT(RECV_PDU(&w->session), &csr) != 0) {
		CRITICAL("Unable to decode GET_CERT PDU");
		return 0;
	}

	/* FIXME: how do we verify CSR integrity? */

	csr_file  = string("%s/%s.csr", w->requests_dir, w->peer);
	cert_store_request(csr, csr_file);
	free(csr_file);

	cert_file = string("%s/%s.pem", w->certs_dir, w->peer);
	cert = cert_retrieve_certificate(cert_file);
	free(cert_file);

	if (pdu_send_SEND_CERT(&w->session, cert) < 0) { return 0; }

	return 1;
}

static int handle_FACTS(worker *w)
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

static int handle_FILE(worker *w)
{
	char hex[SHA1_HEX_DIGEST_SIZE + 1] = {0};
	sha1 checksum;
	struct resource *res;
	struct res_file *file, *match;

	if (!w->peer_verified) {
		WARNING("Unverified peer tried to FILE");
		pdu_send_ERROR(&w->session, 401, "Peer Certificate Required");
		return 1;
	}

	if (RECV_PDU(&w->session)->len != SHA1_HEX_DIGEST_SIZE) {
		pdu_send_ERROR(&w->session, 400, "Malformed FILE request");
		return 1;
	}

	memcpy(hex, RECV_PDU(&w->session)->data, SHA1_HEX_DIGEST_SIZE);
	sha1_init(&checksum, hex);

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
			if (sha1_cmp(&match->rf_rsha1, &checksum) == 0) {
				file = match;
				break;
			}
		}
	}

	if (file) {
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

static int handle_REPORT(worker *w)
{
	struct job *job;
	rowid host_id;
	DB *db = NULL;

	if (!w->peer_verified) {
		WARNING("Unverified peer tried to REPORT");
		pdu_send_ERROR(&w->session, 401, "Peer Certificate Required");
		return 1;
	}

	if (pdu_decode_REPORT(RECV_PDU(&w->session), &job) != 0) {
		CRITICAL("Unable to decode REPORT");
		return 0;
	}

	db = db_open(DB_MASTER, w->db_file);
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
	db_close(db); db = NULL;

	if (pdu_send_BYE(&w->session) < 0) { return 0; }

	return 1;

failed:
	if (db) { db_close(db); }
	return 0;
}

static int handle_unknown(worker *w)
{
	char *message;
	protocol_op op = RECV_PDU(&w->session)->op;

	message = string("Protocol Error: Unrecognized PDU op %s(%u)", protocol_op_name(op), op);
	WARNING("%s", message);
	pdu_send_ERROR(&w->session, 405, message);
	free(message);

	return 1;
}

static int verify_peer(worker *w)
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

static int send_file(worker *w, const char *path)
{
	int fd = open(path, O_RDONLY);
	int n;
	while ((n = pdu_send_DATA(&w->session, fd, NULL)) > 0)
		;

	return n;
}

static int send_template(worker *w, const char *path)
{
	struct template *t;
	char *data, *p;
	int n;

	t = template_create(path, w->facts);
	p = data = template_render(t);

	while ((n = pdu_send_DATA(&w->session, -1, p)) > 0) {
		p += n;
	}

	free(t); free(data);
	return n;
}

static void sighup_handler(int signum, siginfo_t *info, void *udata)
{
	pthread_t tid;
	void *status;

	DEBUG("SIGHUP handler entered");

	pthread_create(&tid, NULL, manager_thread, NULL);
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
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
}

static void show_config(server *s)
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
	printf("port          = %s\n", s->port);
	printf("\n");
	printf("log_level     = %s\n", log_level_name(s->log_level));
	printf("\n");
}

static int server_init(server *s)
{
	assert(s);

	struct sigaction sig;

	if (manifest) {
		ERROR("server module already initialized");
		return -1;
	}

	s->log_level = log_set(s->log_level);
	INFO("Log level is %s (%u)", log_level_name(s->log_level), s->log_level);

	pthread_mutex_lock(&manifest_mutex);
		manifest_file = s->manifest_file;
		manifest = parse_file(manifest_file);
	pthread_mutex_unlock(&manifest_mutex);

	if (server_init_ssl(s) != 0) { return -1; }

	/* sig handlers */
	INFO("setting up signal handlers");
	sig.sa_sigaction = sighup_handler;
	sig.sa_flags = SA_SIGINFO;
	sigemptyset(&sig.sa_mask);
	if (sigaction(SIGHUP, &sig, NULL) != 0) {
		ERROR("Unable to set signal handlers: %s", strerror(errno));
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

	/* bind socket */
	INFO("binding SSL socket on port %s", s->port);
	s->listener = BIO_new_accept(strdup(s->port));
	if (!s->listener || BIO_do_accept(s->listener) <= 0) {
		return -1;
	}

	return 0;
}

static int save_facts(worker *w)
{
	char filename[100];
	char *path;
	time_t ts;
	struct tm *now;
	FILE *out;
	struct stat dir;

	/* generate the filename
	   create the path in cache dir
	   open the file
	   save the facts
	   close the file
	   return 0 */

	ts = time(NULL);
	if (!(now = localtime(&ts))) {
		DEBUG("save_facts: localtime failed in generating file name");
		return -1;
	}
	if (strftime(filename, 100, "%Y-%m-%dT%H-%M-%S.facts", now) == 0) {
		DEBUG("save_facts: strftime failed in generating file name");
		return -1;
	}

	path = string("%s/facts/%s", w->cache_dir, w->peer);
	errno = 0;
	if (mkdir(path, 0700) != 0 && errno != EEXIST) {
		free(path);
		DEBUG("save_facts: mkdir failed in generating file");
		return -2;
	}
	free(path);

	path = string("%s/facts/%s/%s", w->cache_dir, w->peer, filename);
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
	worker *w = (worker*)arg;
	int done = 0;

	pthread_detach(pthread_self());
	if (worker_prep(w) != 0) { goto die; }

	/* dispatch */
	while (!done) {
		pdu_receive(&w->session);
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
	free(w);

	return NULL;
}

static void* manager_thread(void *arg)
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

static int server_init_ssl(server *s)
{
	assert(s->ca_cert_file);
	assert(s->cert_file);
	assert(s->key_file);

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

