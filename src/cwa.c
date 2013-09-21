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

#define CLIENT_SPACE

#include "clockwork.h"

#include <getopt.h>
#include <sys/types.h>
#include <sys/time.h>

#include "proto.h"
#include "policy.h"
#include "userdb.h"
#include "client.h"
#include "resources.h"
#include "augcw.h"
#include "db.h"

static struct client* cwa_options(int argc, char **argv);
static void show_version(void);
static void show_help(void);
static void show_compilation_options(void);

static int get_policy(struct client *c);
static int get_file(struct session *session, struct SHA1 *checksum, int fd);
static int enforce_policy(struct client *c, struct job *job);
static int autodetect_managers(struct resource_env *env, const struct hash *facts);
static void print_report(FILE *io, struct report *r);
static int print_summary(FILE *io, struct job *job);
static int send_report(struct client *c, struct job *job);
static int save_report(struct client *c, struct job *job);

/**************************************************************/

int main(int argc, char **argv)
{
	struct client *c;
	struct job *job;

	c = cwa_options(argc, argv);
	if (client_options(c) != 0) {
		fprintf(stderr, "Unable to process client options");
		exit(2);
	}
	c->log_level = log_set(c->log_level);
	INFO("Log level is %s (%u)", log_level_name(c->log_level), c->log_level);
	INFO("Running in mode %u", c->mode);

	INFO("Gathering facts");
	c->facts = hash_new();
	if (fact_gather(c->gatherers, c->facts) != 0) {
		CRITICAL("Unable to gather facts");
		exit(1);
	}

	if (c->mode == CLIENT_MODE_FACTS) {
		fact_write(stdout, c->facts);
		exit(0);
	}

	if (client_connect(c,0) != 0) {
		exit(1);
	}

	if (client_hello(c) != 0) {
		ERROR("Server-side verification failed.");
		printf("Local certificate not found.  Run cwcert(1)\n");

		client_bye(c);
		exit(2);
	}

	job = job_new();
	get_policy(c);
	enforce_policy(c, job);
	print_summary(stdout, job);
	if (c->mode != CLIENT_MODE_TEST) {
		if (save_report(c, job) != 0) {
			WARNING("Unable to store report in local database.");
		}
		if (send_report(c, job) != 0) {
			WARNING("Unable to send report to master database.");
		}
	}

	client_bye(c);
	return 0;
}

/**************************************************************/

static struct client* cwa_options(int argc, char **argv)
{
	struct client *c;

	const char *short_opts = "h?c:s:p:nvqFDV";
	struct option long_opts[] = {
		{ "help",     no_argument,       NULL, 'h' },
		{ "config",   required_argument, NULL, 'c' },
		{ "server",   required_argument, NULL, 's' },
		{ "port",     required_argument, NULL, 'p' },
		{ "dry-run",  no_argument,       NULL, 'n' },
		{ "verbose",  no_argument,       NULL, 'v' },
		{ "quiet",    no_argument,       NULL, 'q' },
		{ "facts",    no_argument,       NULL, 'F' },
		{ "defaults", no_argument,       NULL, 'D' },
		{ "version",  no_argument,       NULL, 'V' },
		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;

	c = xmalloc(sizeof(struct client));

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch(opt) {
		case 'h':
		case '?':
			show_help();
			exit(0);
		case 'V':
			show_version();
			exit(0);
		case 'D':
			show_compilation_options();
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
			c->mode = CLIENT_MODE_TEST;
			break;
		case 'v':
			c->log_level++;
			break;
		case 'q':
			c->log_level--;
			break;
		case 'F':
			c->mode = CLIENT_MODE_FACTS;
			break;
		}
	}

	return c;
}

static void show_version(void)
{
	printf("cwa (Clockwork) " VERSION "\n"
	       "Copyright 2011-2013 James Hunt\n");
}

static void show_help(void)
{
	printf("USAGE: cwa [OPTIONS]\n"
	       "\n"
	       "  -h, --help            Show this helpful message.\n"
	       "                        (for more in-depth help, check the man pages.)\n"
	       "\n"
	       "  -V, --version         Print version and copyright information.\n"
	       "\n"
	       "  -n, --dry-run         Do not enforce the policy locally, just verify policy\n"
	       "                        retrieval and fact gathering.\n"
	       "\n"
	       "  -c, --config          Specify the path to an alternate configuration file.\n"
	       "\n"
	       "  -s, --server          Override the name (IP or DNS) of the policy master.\n"
	       "\n"
	       "  -p, --port            Override the TCP port number to connect to.\n"
	       "\n"
	       "  -D, --defaults        Dump compiled-in default values (paths, ports, etc.)\n"
	       "\n");
}

#define DUMP_COMPILE_OPT(c) printf(" -D %s=\"%s\"\n", #c, c)
static void show_compilation_options(void)
{
	printf("Clockwork v%s compiled with...\n", VERSION);
	DUMP_COMPILE_OPT(SYS_PASSWD);
	DUMP_COMPILE_OPT(SYS_GROUP);
	DUMP_COMPILE_OPT(SYS_SHADOW);
	DUMP_COMPILE_OPT(SYS_GSHADOW);

	DUMP_COMPILE_OPT(CW_VAR_DIR);
	DUMP_COMPILE_OPT(CW_ETC_DIR);
	DUMP_COMPILE_OPT(CW_DATA_DIR);
	DUMP_COMPILE_OPT(CW_LIB_DIR);
	DUMP_COMPILE_OPT(CW_CACHE_DIR);

	DUMP_COMPILE_OPT(AUGEAS_ROOT);
	DUMP_COMPILE_OPT(AUGEAS_INCLUDES);
	DUMP_COMPILE_OPT(HELP_FILES_DIR);

	DUMP_COMPILE_OPT(DEFAULT_POLICYD_CONF);
	DUMP_COMPILE_OPT(DEFAULT_CWA_CONF);
	DUMP_COMPILE_OPT(DEFAULT_MANIFEST_POL);

	DUMP_COMPILE_OPT(DEFAULT_SSL_CA_CERT_FILE);
	DUMP_COMPILE_OPT(DEFAULT_SSL_CRL_FILE);
	DUMP_COMPILE_OPT(DEFAULT_SSL_CERT_FILE);
	DUMP_COMPILE_OPT(DEFAULT_SSL_REQUEST_FILE);
	DUMP_COMPILE_OPT(DEFAULT_SSL_KEY_FILE);
	DUMP_COMPILE_OPT(DEFAULT_SSL_REQUESTS_DIR);
	DUMP_COMPILE_OPT(DEFAULT_SSL_CERTS_DIR);

	DUMP_COMPILE_OPT(DEFAULT_POLICYD_LOCK_FILE);
	DUMP_COMPILE_OPT(DEFAULT_POLICYD_PID_FILE);

	DUMP_COMPILE_OPT(DEFAULT_MASTER_DB_FILE);
	DUMP_COMPILE_OPT(DEFAULT_AGENT_DB_FILE);

	DUMP_COMPILE_OPT(DEFAULT_GATHERER_DIR);
	DUMP_COMPILE_OPT(DEFAULT_SERVER_NAME);
	DUMP_COMPILE_OPT(DEFAULT_SERVER_PORT);
	DUMP_COMPILE_OPT(DEFAULT_POLICYD_LISTEN);

	DUMP_COMPILE_OPT(CACHED_FACTS_DIR);
}

static int get_policy(struct client *c)
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

static int get_file(struct session *session, struct SHA1 *checksum, int fd)
{
	size_t bytes = 0, n;
	int error = 0;

	INFO("Requesting file %s from policy master", checksum->hex);
	if (pdu_send_FILE(session, checksum) != 0) { return -1; }

	do {
		if (pdu_receive(session) != 0) {
			return -1;
		}
		if (RECV_PDU(session)->op != PROTOCOL_OP_DATA) {
			pdu_send_ERROR(session, 505, "Protocol Error");
			return -1;
		}

		n = write(fd, RECV_PDU(session)->data, RECV_PDU(session)->len);
		if (n != RECV_PDU(session)->len) {
			error = 1;
		}
		bytes += n;
	} while(n > 0);

	return (error == 0 ? bytes : -1 * error);
}

static int enforce_policy(struct client *c, struct job *job)
{
	struct resource_env env;
	struct resource *res;
	struct res_file *rf;

	struct report *r;
	int pipefd[2];
	ssize_t bytes = 0;

	if (job_start(job) != 0) {
		CRITICAL("Unable to start job timer");
		exit(2);
	}

	if (autodetect_managers(&env, c->facts) != 0) {
		CRITICAL("Unable to auto-detect appropriate managers.");
		exit(2);
	}

	DEBUG("Initializing Augeas subsystem");
	env.aug_context = augcw_init();
	if (!env.aug_context) {
		CRITICAL("Unable to initialize Augeas subsystem");
		exit(2);
	}

	if ((env.user_pwdb  = pwdb_init(SYS_PASSWD)) == NULL
	 || (env.user_spdb  = spdb_init(SYS_SHADOW)) == NULL
	 || (env.group_grdb = grdb_init(SYS_GROUP))  == NULL
	 || (env.group_sgdb = sgdb_init(SYS_GSHADOW)) == NULL) {
		CRITICAL("Unable to initialize user / group database(s)");
		exit(2);
	}

	if (c->mode == CLIENT_MODE_TEST) {
		INFO("Enforcement skipped (--dry-run specified)");
	} else {
		INFO("Enforcing policy on local system");
	}

	/* Remediate all */
	for_each_resource(res, c->policy) {
		DEBUG("Fixing up %s", res->key);

		env.file_fd = -1;
		env.file_len = 0;
		if (resource_stat(res, &env) != 0) {
			CRITICAL("Failed to stat %s", res->key);
			exit(2);
		}

		if (res->type == RES_FILE) {
			rf = (struct res_file*)(res->resource);
			if (DIFFERENT(rf, RES_FILE_SHA1)) {
				if (pipe(pipefd) != 0) {
					pipefd[0] = -1;
				} else {
					DEBUG("Attempting to retrieve file contents for %s", rf->rf_lpath);
					bytes = get_file(&c->session, &rf->rf_rsha1, pipefd[1]);
					if (bytes < 0) {
						close(pipefd[0]);
						close(pipefd[1]);

						pipefd[0] = -1;
					}
				}
				env.file_fd = pipefd[0];
				env.file_len = bytes;
			}
		}

		r = resource_fixup(res, c->mode == CLIENT_MODE_TEST, &env);
		if (r->compliant && r->fixed) {
			policy_notify(c->policy, res);
		}

		if (res->type == RES_FILE) {
			close(env.file_fd);
		}
		list_add_tail(&r->l, &job->reports);
	}

	if (c->mode != CLIENT_MODE_TEST) {
		pwdb_write(env.user_pwdb,  SYS_PASSWD);
		spdb_write(env.user_spdb,  SYS_SHADOW);
		grdb_write(env.group_grdb, SYS_GROUP);
		sgdb_write(env.group_sgdb, SYS_GSHADOW);

		if (aug_save(env.aug_context) != 0) {
			CRITICAL("augeas save failed; sub-config resources not properly saved!");
			augcw_errors(env.aug_context);
			exit(2);
		}
		aug_close(env.aug_context);
	}

	if (job_end(job) != 0) {
		CRITICAL("Unable to stop job timer");
		exit(2);
	}

	return 0;
}

static int autodetect_managers(struct resource_env *env, const struct hash *facts)
{
	assert(env); // LCOV_EXCL_LINE
	assert(facts); // LCOV_EXCL_LINE

	/* FIXME: allow local installations to override clockwork.manager.* */
	const char *sm = hash_get(facts, "clockwork.manager.service");
	const char *pm = hash_get(facts, "clockwork.manager.package");

	if (!sm || !pm) { return -1; }

	INFO("Using '%s' service manager", sm);
	if (strcmp(sm, "debian") == 0) {
		env->service_manager = SM_debian;
	} else if (strcmp(sm, "chkconfig") == 0) {
		env->service_manager = SM_chkconfig;
	} else {
		WARNING("Unrecognized clockwork.manager.service: '%s'", sm);
		env->service_manager = NULL;
	}

	INFO("Using '%s' package manager", pm);
	if (strcmp(pm, "dpkg_apt") == 0) {
		env->package_manager = PM_dpkg_apt;
	} else if (strcmp(pm, "rpm_yum") == 0) {
		env->package_manager = PM_rpm_yum;
	} else {
		WARNING("Unrecognized clockwork.manager.package: '%s'", pm);
		env->package_manager = NULL;
	}

	return (env->package_manager && env->service_manager ? 0 : -1);
}

static void print_report(FILE *io, struct report *r)
{
	char buf[80];
	struct action *a;

	if (snprintf(buf, 67, "%s: %s", r->res_type, r->res_key) > 67) {
		memcpy(buf + 67 - 1 - 3, "...", 3);
	}
	buf[66] = '\0';

	fprintf(io, "%-66s%14s\n", buf, (r->compliant ? (r->fixed ? "FIXED": "OK") : "NON-COMPLIANT"));

	for_each_action(a, r) {
		memcpy(buf, a->summary, 70);
		buf[70] = '\0';

		if (strlen(a->summary) > 70) {
			memcpy(buf + 71 - 1 - 3, "...", 3);
		}

		fprintf(io, " - %-70s%7s\n", buf,
		        (a->result == ACTION_SUCCEEDED ? "done" :
		         (a->result == ACTION_FAILED ? "failed" : "skipped")));
	}
}

static int print_summary(FILE *io, struct job *job)
{
	struct report *r;
	size_t ok = 0, fixed = 0, non = 0;

	for_each_report(r, job) {
		if (r->compliant && !r->fixed) {
			print_report(io, r);
			ok++;
		}
	}
	fprintf(io, "\n");

	for_each_report(r, job) {
		if (!r->compliant || r->fixed) {
			print_report(io, r);
			fprintf(io, "\n");

			if (r->fixed) {
				fixed++;
			} else {
				non++;
			}
		}
	}

	fprintf(io, "%u resource(s); %u OK, %u fixed, %u non-compliant\n",
	        (unsigned int)(ok+fixed+non), (unsigned int)ok,
	        (unsigned int)fixed, (unsigned int)non);
	fprintf(io, "run duration: %0.2fs\n", job->duration / 1000000.0);

	return 0;
}

static int send_report(struct client *c, struct job *job)
{
	if (pdu_send_REPORT(&c->session, job) < 0) { goto disconnect; }
	if (pdu_receive(&c->session) < 0) {
		goto disconnect;
	}

	if (RECV_PDU(&c->session)->op != PROTOCOL_OP_BYE) {
		CRITICAL("Unexpected op from server: %u", RECV_PDU(&c->session)->op);
		pdu_send_ERROR(&c->session, 505, "Protocol Error");
		goto disconnect;
	}

	return 0;

disconnect:
	DEBUG("send_report forcing a disconnect");
	client_disconnect(c);
	exit(1);
}

static int save_report(struct client *c, struct job *job)
{
	struct db *db;
	db = db_open(AGENTDB, c->db_file);
	if (!db) {
		return -1;
	}

	if (agentdb_store_report(db, job) != 0) {
		db_close(db);
		return -1;
	}

	db_close(db);
	return 0;
}
