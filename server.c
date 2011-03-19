#include "clockwork.h"

#include "server.h"
#include "config/parser.h"

/**************************************************************/

static server default_options = {
	.daemonize = SERVER_OPT_TRUE,
	.debug     = SERVER_OPT_FALSE,
	.log_level = LOG_LEVEL_ERROR,

	.config_file   = "/etc/clockwork/policyd.conf",
	.manifest_file = "/etc/clockwork/manifest.pol",

	.lock_file = "/var/lock/policyd",
	.pid_file  = "/var/run/policyd",

	.ca_cert_file = "/etc/clockwork/ssl/CA.pem",
	.crl_file     = "/etc/clockwork/ssl/revoked.pem",
	.cert_file    = "/etc/clockwork/ssl/cert.pem",
	.key_file     = "/etc/clockwork/ssl/key.pem",

	.requests_dir = "/etc/clockwork/ssl/pending",
	.certs_dir    = "/etc/clockwork/ssl/signed",

	.port = "7890"
};

/**************************************************************/

static server* configured_options(const char *path);
static int merge_servers(server *a, server *b);

/**********************************************************/

static server* configured_options(const char *path)
{
	server *s;
	struct hash *config;
	char *v;

	s = xmalloc(sizeof(server));

	config = parse_config(path);
	if (config) {
		v = hash_get(config, "ca_cert_file");
		if (v) { s->ca_cert_file = strdup(v); }

		v = hash_get(config, "crl_file");
		if (v) { s->crl_file = strdup(v); }

		v = hash_get(config, "cert_file");
		if (v) { s->cert_file = strdup(v); }

		v = hash_get(config, "key_file");
		if (v) { s->key_file = strdup(v); }

		v = hash_get(config, "requests_dir");
		if (v) { s->requests_dir = strdup(v); }

		v = hash_get(config, "certs_dir");
		if (v) { s->certs_dir = strdup(v); }

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
			} else if (strcmp(v, "info") == 0) {
				s->log_level = LOG_LEVEL_INFO;
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
static int merge_servers(server *a, server *b)
{
	MERGE_BOOLEAN_OPTION(a,b,debug);
	MERGE_BOOLEAN_OPTION(a,b,daemonize);

	/* note: log_level handled differently */

	MERGE_STRING_OPTION(a,b,config_file);
	MERGE_STRING_OPTION(a,b,manifest_file);
	MERGE_STRING_OPTION(a,b,lock_file);
	MERGE_STRING_OPTION(a,b,pid_file);
	MERGE_STRING_OPTION(a,b,ca_cert_file);
	MERGE_STRING_OPTION(a,b,crl_file);
	MERGE_STRING_OPTION(a,b,cert_file);
	MERGE_STRING_OPTION(a,b,key_file);
	MERGE_STRING_OPTION(a,b,requests_dir);
	MERGE_STRING_OPTION(a,b,certs_dir);
	MERGE_STRING_OPTION(a,b,port);

	return 0;
}
#undef MERGE_STRING_OPTION
#undef MERGE_BOOLEAN_OPTION

/**********************************************************/

int server_options(server *args)
{
	server *cfg;
	cfg = configured_options(args->config_file ? args->config_file : default_options.config_file);

	if (merge_servers(args, cfg) != 0
	 || merge_servers(args, &default_options) != 0) {
		return 1;
	}
	args->log_level += (cfg->log_level == 0 ? default_options.log_level : cfg->log_level);

	return 0;
}

