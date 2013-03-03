/*
  Copyright 2011-2013 James Hunt <james@jameshunt.us>

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

#include "server.h"
#include "conf/parser.h"

/**************************************************************/

static struct server default_options = {
	.daemonize     = SERVER_OPT_TRUE,
	.debug         = SERVER_OPT_FALSE,
	.log_level     = LOG_LEVEL_ERROR,

	.config_file   = DEFAULT_POLICYD_CONF,
	.manifest_file = DEFAULT_MANIFEST_POL,
	.lock_file     = DEFAULT_POLICYD_LOCK_FILE,
	.pid_file      = DEFAULT_POLICYD_PID_FILE,
	.ca_cert_file  = DEFAULT_SSL_CA_CERT_FILE,
	.crl_file      = DEFAULT_SSL_CRL_FILE,
	.cert_file     = DEFAULT_SSL_CERT_FILE,
	.key_file      = DEFAULT_SSL_KEY_FILE,
	.requests_dir  = DEFAULT_SSL_REQUESTS_DIR,
	.certs_dir     = DEFAULT_SSL_CERTS_DIR,
	.db_file       = DEFAULT_MASTER_DB_FILE,
	.cache_dir     = CW_CACHE_DIR,
	.listen        = DEFAULT_POLICYD_LISTEN,

	/* Options below here are not available in the
	   configuration file, only as defaults / CLI opts. */

	.show_config = SERVER_OPT_FALSE
};

/**************************************************************/

static void server_free(struct server *s);
static struct server* configured_options(const char *path);
static int merge_servers(struct server *a, struct server *b);

/**********************************************************/

static void server_free(struct server *s)
{
	if (s) {
		xfree(s->config_file);
		xfree(s->manifest_file);
		xfree(s->lock_file);
		xfree(s->pid_file);
		xfree(s->ca_cert_file);
		xfree(s->crl_file);
		xfree(s->cert_file);
		xfree(s->key_file);
		xfree(s->db_file);
		xfree(s->requests_dir);
		xfree(s->certs_dir);
		xfree(s->cache_dir);
		xfree(s->listen);
	}
	xfree(s);
}

static struct server* configured_options(const char *path)
{
	struct server *s;
	struct hash *config;
	char *v;

	s = xmalloc(sizeof(struct server));

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

		v = hash_get(config, "cache_dir");
		if (v) { s->cache_dir = strdup(v); }

		v = hash_get(config, "listen");
		if (v) { s->listen = strdup(v); }

		v = hash_get(config, "lock_file");
		if (v) { s->lock_file = strdup(v); }

		v = hash_get(config, "pid_file");
		if (v) { s->pid_file = strdup(v); }

		v = hash_get(config, "manifest_file");
		if (v) { s->manifest_file = strdup(v); }

		v = hash_get(config, "db_file");
		if (v) { s->db_file = strdup(v); }

		v = hash_get(config, "log_level");
		if (v) {
			if (strcasecmp(v, "critical") == 0) {
				s->log_level = LOG_LEVEL_CRITICAL;
			} else if (strcasecmp(v, "error") == 0) {
				s->log_level = LOG_LEVEL_ERROR;
			} else if (strcasecmp(v, "warning") == 0) {
				s->log_level = LOG_LEVEL_WARNING;
			} else if (strcasecmp(v, "notice") == 0) {
				s->log_level = LOG_LEVEL_NOTICE;
			} else if (strcasecmp(v, "info") == 0) {
				s->log_level = LOG_LEVEL_INFO;
			} else if (strcasecmp(v, "debug") == 0) {
				s->log_level = LOG_LEVEL_DEBUG;
			} else if (strcasecmp(v, "all") == 0) {
				s->log_level = LOG_LEVEL_ALL;
			} else { // handle "none" implicitly
				s->log_level = LOG_LEVEL_NONE;
			}
		}

		hash_free_all(config);
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
static int merge_servers(struct server *a, struct server *b)
{
	MERGE_BOOLEAN_OPTION(a,b,debug);
	MERGE_BOOLEAN_OPTION(a,b,daemonize);
	MERGE_BOOLEAN_OPTION(a,b,show_config);

	/* note: log_level handled differently */

	MERGE_STRING_OPTION(a,b,config_file);
	MERGE_STRING_OPTION(a,b,manifest_file);
	MERGE_STRING_OPTION(a,b,lock_file);
	MERGE_STRING_OPTION(a,b,pid_file);
	MERGE_STRING_OPTION(a,b,ca_cert_file);
	MERGE_STRING_OPTION(a,b,crl_file);
	MERGE_STRING_OPTION(a,b,cert_file);
	MERGE_STRING_OPTION(a,b,key_file);
	MERGE_STRING_OPTION(a,b,db_file);
	MERGE_STRING_OPTION(a,b,requests_dir);
	MERGE_STRING_OPTION(a,b,certs_dir);
	MERGE_STRING_OPTION(a,b,cache_dir);
	MERGE_STRING_OPTION(a,b,listen);

	return 0;
}
#undef MERGE_STRING_OPTION
#undef MERGE_BOOLEAN_OPTION

/**********************************************************/

int server_options(struct server *args)
{
	struct server *cfg;
	cfg = configured_options(args->config_file ? args->config_file : default_options.config_file);

	if (merge_servers(args, cfg) != 0
	 || merge_servers(args, &default_options) != 0) {
		server_free(cfg);
		return 1;
	}
	args->log_level += (cfg->log_level == 0 ? default_options.log_level : cfg->log_level);
	server_free(cfg);

	return 0;
}

