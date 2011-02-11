#include "clockwork.h"
#include "spec/parser.h"
#include "config/parser.h"
#include "server.h"

#include <getopt.h>

/**************************************************************/

static server default_opts = {
	.daemonize = SERVER_OPT_TRUE,
	.debug     = SERVER_OPT_FALSE,
	.log_level = LOG_LEVEL_ERROR,

	.config_file   = DEFAULT_CONFIG_FILE,
	.manifest_file = "/etc/clockwork/manifest.pol",

	.lock_file = "/var/lock/policyd",
	.pid_file  = "/var/run/policyd",

	.ca_cert_file = "/etc/clockwork/ssl/CA.pem",
	.cert_file    = "/etc/clockwork/ssl/cert.pem",
	.key_file     = "/etc/clockwork/ssl/key.pem",

	.port = "7890"
};

static server* config_file_options(const char *path);
static server* command_line_options(int argc, char **argv);
static int merge_options(server *a, server *b);

#ifndef NDEBUG
static void dump_options(const char *prefix, server *s);
#else
# define dump_options(p,s)
#endif
static void show_help(void);

/**************************************************************/

int main(int argc, char **argv)
{
	server *arg_opts, *cfg_opts;

	arg_opts = command_line_options(argc, argv);
	cfg_opts = config_file_options(arg_opts->config_file ? arg_opts->config_file : default_opts.config_file);

	dump_options("arg_opts",      arg_opts);
	dump_options("cfg_opts",      cfg_opts);
	dump_options("default_opts", &default_opts);

	if (merge_options(arg_opts, cfg_opts) != 0
	 || merge_options(arg_opts, &default_opts) != 0) {
		fprintf(stderr, "Unable to process server options");
		exit(2);
	}
	/* arg_opts is a modifier to the base log level */
	arg_opts->log_level += (cfg_opts->log_level == 0 ? default_opts.log_level : cfg_opts->log_level);

	dump_options("merged_opts", arg_opts);

	INFO("policyd starting up");
	if (server_init(arg_opts) != 0) {
		CRITICAL("Failed to initialize policyd server thread");
		exit(2);
	}
	DEBUG("entering server_loop");
	server_loop(arg_opts);
	server_deinit(arg_opts);

	return 0;
}

/**************************************************************/

static server* config_file_options(const char *path)
{
	server *s;
	struct hash *config;
	char *v;

	s = xmalloc(sizeof(server));

	config = parse_config(path);
	if (config) {
		v = hash_get(config, "ca_cert_file");
		if (v) { s->ca_cert_file = strdup(v); }

		v = hash_get(config, "cert_file");
		if (v) { s->cert_file = strdup(v); }

		v = hash_get(config, "key_file");
		if (v) { s->key_file = strdup(v); }

		v = hash_get(config, "port");
		if (v) { s->port = strdup(v); }

		v = hash_get(config, "lock_file");
		if (v) { s->lock_file = strdup(v); }

		v = hash_get(config, "pid_file");
		if (v) { s->pid_file = strdup(v); }

		v = hash_get(config, "manifest_file");
		if (v) { s->manifest_file = strdup(v); }

		v = hash_get(config, "log_level");
		if (v) {
			if (strcmp(v, "critical") == 0) {
				s->log_level = LOG_LEVEL_CRITICAL;
			} else if (strcmp(v, "error") == 0) {
				s->log_level = LOG_LEVEL_ERROR;
			} else if (strcmp(v, "warning") == 0) {
				s->log_level = LOG_LEVEL_WARNING;
			} else if (strcmp(v, "notice") == 0) {
				s->log_level = LOG_LEVEL_NOTICE;
			} else if (strcmp(v, "debug") == 0) {
				s->log_level = LOG_LEVEL_DEBUG;
			} else if (strcmp(v, "all") == 0) {
				s->log_level = LOG_LEVEL_ALL;
			} else { // handle "none" implicitly
				s->log_level = LOG_LEVEL_NONE;
			}
		}
	}

	return s;
}

static server* command_line_options(int argc, char **argv)
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
		case 'p':
			free(s->port);
			s->port = strdup(optarg);
			break;
		}
	}

	return s;
}

#define MERGE_STRING_OPTION(a,b,opt) do {\
	if (!a->opt && b->opt) {\
		a->opt = strdup(b->opt);\
	}\
} while(0)
#define MERGE_BOOLEAN_OPTION(a,b,opt) do { \
	if (a->opt == SERVER_OPT_UNSPEC) {\
		a->opt = b->opt;\
	}\
} while(0)
static int merge_options(server *a, server *b)
{
	MERGE_BOOLEAN_OPTION(a,b,debug);
	MERGE_BOOLEAN_OPTION(a,b,daemonize);

	/* note: log_level handled differently */

	MERGE_STRING_OPTION(a,b,config_file);
	MERGE_STRING_OPTION(a,b,manifest_file);
	MERGE_STRING_OPTION(a,b,lock_file);
	MERGE_STRING_OPTION(a,b,pid_file);
	MERGE_STRING_OPTION(a,b,ca_cert_file);
	MERGE_STRING_OPTION(a,b,cert_file);
	MERGE_STRING_OPTION(a,b,key_file);
	MERGE_STRING_OPTION(a,b,port);

	return 0;
}
#undef MERGE_STRING_OPTION
#undef MERGE_BOOLEAN_OPTION

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
	       "                        If not given, defaults to %s\n"
	       "\n"
	       "  -p, --port            Override the TCP port number that policyd should\n"
	       "                        bind to and listen on.\n"
	       "\n",
	       DEFAULT_CONFIG_FILE);
}

#ifndef NDEBUG
static void dump_options(const char *prefix, server *s)
{
	fprintf(stderr, "%s {\n", (prefix ? prefix : "server_options"));
	fprintf(stderr, "  debug         = %i\n",   s->debug);
	fprintf(stderr, "  log_level     = %i\n",   s->log_level);
	fprintf(stderr, "\n");
	fprintf(stderr, "  config_file   = '%s'\n", s->config_file);
	fprintf(stderr, "  manifest_file = '%s'\n", s->manifest_file);
	fprintf(stderr, "\n");
	fprintf(stderr, "  daemonize     = %i\n",   s->daemonize);
	fprintf(stderr, "  lock_file     = '%s'\n", s->lock_file);
	fprintf(stderr, "  pid_file      = '%s'\n", s->pid_file);
	fprintf(stderr, "\n");
	fprintf(stderr, "  ca_cert_file  = '%s'\n", s->ca_cert_file);
	fprintf(stderr, "  cert_file     = '%s'\n", s->cert_file);
	fprintf(stderr, "  key_file      = '%s'\n", s->key_file);
	fprintf(stderr, "\n");
	fprintf(stderr, "  port          = '%s'\n", s->port);
	fprintf(stderr, "}\n\n");
}
#endif

