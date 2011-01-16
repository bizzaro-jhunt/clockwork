#include "clockwork.h"

#include <getopt.h>

#include "config/parser.h"
#include "proto.h"
#include "policy.h"
#include "userdb.h"
#include "client.h"

static client default_opts = {
	.log_level = LOG_LEVEL_CRITICAL,
	.dryrun    = 0,

	.config_file = DEFAULT_CONFIG_FILE,

	.ca_cert_file = "/etc/clockwork/ssl/CA.pem",
	.cert_file    = "/etc/clockwork/ssl/cert.pem",
	.key_file     = "/etc/clockwork/ssl/key.pem",

	.gatherers = "/etc/clockwork/gather.d/*",

	.s_address = "clockwork",
	.s_port    = "7890"
};

static client* config_file_options(const char *path);
static client* command_line_options(int argc, char **argv);
static int merge_options(client *a, client *b);

static void show_help(void);

#ifndef NDEBUG
static void dump_options(const char *prefix, client *c);
static void dump_policy(struct policy *pol);
static void policy_check(struct policy *pol, protocol_session *session);
#else
# define dump_options(p,s)
# define dump_policy(p)
# define policy_check(p,s)
#endif
static void show_help(void);

/**************************************************************/

int main(int argc, char **argv)
{
	client *arg_opts, *cfg_opts;
	LIST(report);

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

	log_level(arg_opts->log_level);
	if (client_init(arg_opts) != 0) {
		exit(1);
	}

	client_get_policy(arg_opts); /* FIXME: check return value */

	dump_policy(arg_opts->policy);
	policy_check(arg_opts->policy, &arg_opts->session);

	if (arg_opts->dryrun) {
		INFO("Enforcement skipped (--dry-run specified)\n");
	} else {
		INFO("Enforcing policy on local system");
	}
	client_enforce_policy(arg_opts, &report);
	client_print_report(stdout, &report);

	client_deinit(arg_opts);
	return 0;
}

/**************************************************************/

static client* config_file_options(const char *path)
{
	client *c;
	struct hash *config;
	char *v;

	c = calloc(1, sizeof(client));
	if (!c) {
		return NULL;
	}

	config = parse_config(path);
	if (config) {
		v = hash_get(config, "ca_cert_file");
		if (v) { c->ca_cert_file = strdup(v); }

		v = hash_get(config, "cert_file");
		if (v) { c->cert_file = strdup(v); }

		v = hash_get(config, "key_file");
		if (v) { c->key_file = strdup(v); }

		v = hash_get(config, "gatherers");
		if (v) { c->gatherers = strdup(v); }

		v = hash_get(config, "server");
		if (v) { c->s_address = strdup(v); }

		v = hash_get(config, "port");
		if (v) { c->s_port = strdup(v); }
	}

	return c;
}

static client* command_line_options(int argc, char **argv)
{
	client *c;

	const char *short_opts = "h?c:s:p:nvq";
	struct option long_opts[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "config", required_argument, NULL, 'c' },
		{ "server", required_argument, NULL, 's' },
		{ "port",   required_argument, NULL, 'p' },
		{ "dry-run", no_argument,      NULL, 'n' },
		{ "verbose", no_argument,      NULL, 'v' },
		{ "quiet",  no_argument,       NULL, 'q' },
		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;

	c = calloc(1, sizeof(client));
	if (!c) {
		return NULL;
	}

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch(opt) {
		case 'h':
		case '?':
			show_help();
			exit(0);
		case 'c':
			free(c->config_file);
			c->config_file = strdup(optarg);
			break;
		case 's':
			free(c->s_address);
			c->s_address = strdup(optarg);
			break;
		case 'p':
			free(c->s_port);
			c->s_port = strdup(optarg);
			break;
		case 'n':
			c->dryrun = 1;
			break;
		case 'v':
			c->log_level++;
			break;
		case 'q':
			c->log_level--;
			break;
		}
	}

	return c;
}

#define MERGE_STRING_OPTION(a,b,opt) do {\
	if (!a->opt && b->opt) {\
		a->opt = strdup(b->opt);\
	}\
} while(0)
static int merge_options(client *a, client *b)
{
	MERGE_STRING_OPTION(a,b,config_file);
	MERGE_STRING_OPTION(a,b,ca_cert_file);
	MERGE_STRING_OPTION(a,b,cert_file);
	MERGE_STRING_OPTION(a,b,key_file);
	MERGE_STRING_OPTION(a,b,gatherers);
	MERGE_STRING_OPTION(a,b,s_address);
	MERGE_STRING_OPTION(a,b,s_port);

	a->dryrun = (b->dryrun ? 1 : a->dryrun);

	return 0;
}
#undef MERGE_STRING_OPTION

static void show_help(void)
{
	printf( "USAGE: cwa [OPTIONS]\n"
	       "\n"
	       "  -h, --help            Show this helpful message.\n"
	       "                        (for more in-depth help, check the man pages.)\n"
	       "\n"
	       "  -n, --dry-run         Do not enforce the policy locally, just verify policy\n"
	       "                        retrieval and fact gathering.\n"
	       "\n"
	       "  -c, --config          Specify the path to an alternate configuration file.\n"
	       "                        If not given, defaults to " DEFAULT_CONFIG_FILE "\n"
	       "\n"
	       "  -s, --server          Override the name (IP or DNS) of the policy master.\n"
	       "\n"
	       "  -p, --port            Override the TCP port number to connect to.\n"
	       "\n");
}

#ifndef NDEBUG
static void dump_options(const char *prefix, client *c)
{
	fprintf(stderr, "%s {\n", (prefix ? prefix : "client"));
	fprintf(stderr, "  config_file   = '%s'\n", c->config_file);
	fprintf(stderr, "\n");
	fprintf(stderr, "  ca_cert_file  = '%s'\n", c->ca_cert_file);
	fprintf(stderr, "  cert_file     = '%s'\n", c->cert_file);
	fprintf(stderr, "  key_file      = '%s'\n", c->key_file);
	fprintf(stderr, "\n");
	fprintf(stderr, "  gatherers     = '%s'\n", c->gatherers);
	fprintf(stderr, "\n");
	fprintf(stderr, "  s_address     = '%s'\n", c->s_address);
	fprintf(stderr, "  s_port        = '%s'\n", c->s_port);
	fprintf(stderr, "}\n\n");
}

static void dump_policy(struct policy *pol)
{
	char *packed = policy_pack(pol);
	fprintf(stderr, "%s\n", packed);
	free(packed);
}

static void policy_check(struct policy *pol, protocol_session *session)
{
	struct res_file *rf;
	for_each_node(rf, &pol->res_files, res) {
		client_get_file(session, &rf->rf_rsha1);
	}
}
#endif
