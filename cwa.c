#define CLIENT_SPACE

#include "clockwork.h"

#include <getopt.h>
#include <glob.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "proto.h"
#include "policy.h"
#include "userdb.h"
#include "client.h"
#include "pkgmgr.h"

/* FIXME: one-off weirdness for fixup res_file */
#include "res_file.h"

static client* cwa_options(int argc, char **argv);
static void show_help(void);

static int gather_facts_from_script(const char *script, struct hash *facts);
static int gather_facts(client *c);
static int get_policy(client *c);
static int get_file(protocol_session *session, sha1 *checksum, int fd);
static int enforce_policy(client *c, struct list *report);
static int print_report(FILE *io, struct list *report);
static int send_report(client *c);

/**************************************************************/

int main(int argc, char **argv)
{
	client *c;
	LIST(report);

	c = cwa_options(argc, argv);
	if (client_options(c) != 0) {
		fprintf(stderr, "Unable to process server options");
		exit(2);
	}
	c->log_level = log_level(c->log_level);
	INFO("Log level is %s (%u)", log_level_name(c->log_level), c->log_level);

	INFO("Gathering facts");
	if (gather_facts(c) != 0) {
		CRITICAL("Unable to gather facts");
		exit(1);
	}

	if (client_connect(c) != 0) {
		exit(1);
	}

	if (client_hello(c) != 0) {
		ERROR("Server-side verification failed.");
		printf("Local certificate not found.  Run cwcert(1)\n");

		client_bye(c);
		exit(2);
	}

	get_policy(c);
	enforce_policy(c, &report);
	print_report(stdout, &report);
	send_report(c);

	client_bye(c);
	return 0;
}

/**************************************************************/

static client* cwa_options(int argc, char **argv)
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

	c = xmalloc(sizeof(client));

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

static void show_help(void)
{
	printf("USAGE: cwa [OPTIONS]\n"
	       "\n"
	       "  -h, --help            Show this helpful message.\n"
	       "                        (for more in-depth help, check the man pages.)\n"
	       "\n"
	       "  -n, --dry-run         Do not enforce the policy locally, just verify policy\n"
	       "                        retrieval and fact gathering.\n"
	       "\n"
	       "  -c, --config          Specify the path to an alternate configuration file.\n"
	       "\n"
	       "  -s, --server          Override the name (IP or DNS) of the policy master.\n"
	       "\n"
	       "  -p, --port            Override the TCP port number to connect to.\n"
	       "\n");
}

static int gather_facts_from_script(const char *script, struct hash *facts)
{
	pid_t pid;
	int pipefd[2];
	FILE *input;
	char *path_copy, *arg0;

	path_copy = strdup(script);
	arg0 = basename(path_copy);
	free(path_copy);

	INFO("Processing script %s", script);

	if (pipe(pipefd) != 0) {
		perror("gather_facts");
		return -1;
	}

	pid = fork();
	switch (pid) {
	case -1:
		perror("gather_facts: fork");
		return -1;

	case 0: /* in child */
		close(pipefd[0]);
		close(0); close(1); close(2);

		dup2(pipefd[1], 1); /* dup pipe as stdout */

		execl(script, arg0, NULL);
		exit(1); /* if execl returns, we failed */

	default: /* in parent */
		close(pipefd[1]);
		input = fdopen(pipefd[0], "r");

		fact_read(input, facts);
		waitpid(pid, NULL, 0);
		fclose(input);
		close(pipefd[0]);

		return 0;
	}
}

static int gather_facts(client *c)
{
	glob_t scripts;
	size_t i;

	c->facts = hash_new();

	switch(glob(c->gatherers, GLOB_MARK, NULL, &scripts)) {
	case GLOB_NOMATCH:
		globfree(&scripts);
		if (gather_facts_from_script(c->gatherers, c->facts) != 0) {
			hash_free(c->facts);
			return -1;
		}
		return 0;

	case GLOB_NOSPACE:
	case GLOB_ABORTED:
		hash_free(c->facts);
		return -1;

	}

	for (i = 0; i < scripts.gl_pathc; i++) {
		if (gather_facts_from_script(scripts.gl_pathv[i], c->facts) != 0) {
			hash_free(c->facts);
			globfree(&scripts);
			return -1;
		}
	}

	globfree(&scripts);
	return 0;
}

static int get_policy(client *c)
{
	if (pdu_send_FACTS(&c->session, c->facts) < 0) { goto disconnect; }

	if (pdu_receive(&c->session) < 0) {
		CRITICAL("Error: %u - %s", c->session.errnum, c->session.errstr);
		goto disconnect;
	}

	if (RECV_PDU(&c->session)->op != PROTOCOL_OP_POLICY) {
		CRITICAL("Unexpected op from server: %u", RECV_PDU(&c->session)->op);
		pdu_send_ERROR(&c->session, 505, "Protocol Error");
		goto disconnect;
	}

	if (pdu_decode_POLICY(RECV_PDU(&c->session), &c->policy) != 0) {
		CRITICAL("Unable to decode POLICY PDU");
		goto disconnect;
	}

	return 0;

disconnect:
	DEBUG("get_policy forcing a disconnect");
	client_disconnect(c);
	exit(1);
}

static int get_file(protocol_session *session, sha1 *checksum, int fd)
{
	size_t bytes = 0, n = 0;

	if (pdu_send_FILE(session, checksum) != 0) { return -1; }

	do {
		pdu_receive(session);
		if (RECV_PDU(session)->op != PROTOCOL_OP_DATA) {
			pdu_send_ERROR(session, 505, "Protocol Error");
			return -1;
		}

		n = RECV_PDU(session)->len;
		write(fd, RECV_PDU(session)->data, n);
		bytes += n;
	} while(n > 0);

	return bytes;
}

static int enforce_policy(client *c, struct list *l)
{
	struct resource_env env;
	struct resource *res;
	struct res_file *rf;

	struct report *r;
	int pipefd[2];
	ssize_t bytes = 0;

	/* FIXME: hard-coded in cwa */
	env.package_manager = UBUNTU_PACKAGE_MANAGER;

	if ((env.user_pwdb  = pwdb_init(SYS_PASSWD)) == NULL
	 || (env.user_spdb  = spdb_init(SYS_SHADOW)) == NULL
	 || (env.group_grdb = grdb_init(SYS_GROUP))  == NULL
	 || (env.group_sgdb = sgdb_init(SYS_GSHADOW)) == NULL) {
		CRITICAL("Unable to initialize user / group database(s)");
		exit(2);
	}

	if (c->dryrun) {
		INFO("Enforcement skipped (--dry-run specified)");
	} else {
		INFO("Enforcing policy on local system");
	}

	/* Remediate all */
	for_each_node(res, &c->policy->resources, l) {
		/* FIXME: one-off weirdness for res_file */
		env.file_fd = -1;
		env.file_len = 0;
		if (res->type == RES_FILE) {
			rf = (struct res_file*)(res->resource);
			rf->rf_uid = pwdb_lookup_uid(env.user_pwdb,  rf->rf_owner);
			rf->rf_gid = grdb_lookup_gid(env.group_grdb, rf->rf_group);
		}

		resource_stat(res, &env);

		/* FIXME: one-off weirdness for res_file */
		if (res->type == RES_FILE) {
			rf = (struct res_file*)(res->resource);
			if (res_file_different(rf, SHA1)) {
				if (pipe(pipefd) != 0) {
					pipefd[0] = -1;
				} else {
					bytes = get_file(&c->session, &rf->rf_rsha1, pipefd[1]);
					if (bytes <= 0) {
						close(pipefd[0]);
						close(pipefd[1]);

						pipefd[0] = -1;
					}
				}
				env.file_fd = pipefd[0];
				env.file_len = bytes;
			}
		}

		r = resource_fixup(res, c->dryrun, &env);

		if (res->type == RES_FILE) {
			close(env.file_fd);
		}
		list_add_tail(&r->rep, l);
	}

	if (!c->dryrun) {
		pwdb_write(env.user_pwdb,  SYS_PASSWD);
		spdb_write(env.user_spdb,  SYS_SHADOW);
		grdb_write(env.group_grdb, SYS_GROUP);
		sgdb_write(env.group_sgdb, SYS_GSHADOW);
	}

	return 0;
}

static int print_report(FILE *io, struct list *report)
{
	struct report *r;
	size_t ok = 0, fixed = 0, non = 0;

	for_each_node(r, report, rep) {
		if (r->compliant && !r->fixed) {
			report_print(io, r);
			ok++;
		}
	}
	fprintf(io, "\n");

	for_each_node(r, report, rep) {
		if (!r->compliant || r->fixed) {
			report_print(io, r);
			fprintf(io, "\n");

			if (r->fixed) {
				fixed++;
			} else {
				non++;
			}
		}
	}

	fprintf(io, "%u resource(s); %u OK, %u fixed, %u non-compliant\n",
	        (ok+fixed+non), ok, fixed, non);

	return 0;
}

static int send_report(client *c)
{
	return 0;
}
