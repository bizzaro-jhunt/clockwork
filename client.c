#include "client.h"

#include <glob.h>
#include <sys/types.h>
#include <sys/wait.h>

static int gather_facts(const char *script, struct hash *facts);

/**************************************************************/

int client_connect(client *c)
{
	char *addr;
	size_t len;
	long err;

	INFO("Gathering Facts");
	if (client_gather_facts(c) != 0) {
		CRITICAL("Unable to gather facts");
		exit(1);
	}

	protocol_ssl_init();
	c->ssl_ctx = protocol_ssl_default_context(c->ca_cert_file,
	                                          c->cert_file,
	                                          c->key_file);

	if (!c->ssl_ctx) {
		CRITICAL("Error setting up SSL context");
		protocol_ssl_backtrace();
		return -1;
	}

	len = snprintf(NULL, 0, "%s:%s", c->s_address, c->s_port);
	addr = xmalloc(len+1);
	snprintf(addr, len+1, "%s:%s", c->s_address, c->s_port);
	DEVELOPER("Connecting to %s", addr);

	c->socket = BIO_new_connect(addr);
	if (!c->socket || BIO_do_connect(c->socket) <= 0) {
		CRITICAL("Error creating connection BIO");
		protocol_ssl_backtrace();
		return -1;
	}

	if (!(c->ssl = SSL_new(c->ssl_ctx))) {
		CRITICAL("Error creating an SSL context");
		protocol_ssl_backtrace();
		return -1;
	}

	SSL_set_bio(c->ssl, c->socket, c->socket);
	if (SSL_connect(c->ssl) <= 0) {
		CRITICAL("Error connecting SSL object");
		protocol_ssl_backtrace();
		return -1;
	}

	if ((err = protocol_ssl_verify_peer(c->ssl, c->s_address)) != X509_V_OK) {
		CRITICAL("Server certificate verification failed: %s", X509_verify_cert_error_string(err));
		protocol_ssl_backtrace();
		return -1;
	}

	protocol_session_init(&c->session, c->ssl);

	return 0;
}

int client_disconnect(client *c)
{
	if (pdu_send_BYE(&c->session) < 0) {
		perror("client_disconnect");
		return -1;
	}
	pdu_receive(&c->session);
	protocol_session_deinit(&c->session);

	SSL_shutdown(c->ssl);
	SSL_free(c->ssl);
	SSL_CTX_free(c->ssl_ctx);
	return 0;
}

int client_gather_facts(client *c)
{
	glob_t scripts;
	size_t i;

	c->facts = hash_new();

	switch(glob(c->gatherers, GLOB_MARK, NULL, &scripts)) {
	case GLOB_NOMATCH:
		globfree(&scripts);
		if (gather_facts(c->gatherers, c->facts) != 0) {
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
		if (gather_facts(scripts.gl_pathv[i], c->facts) != 0) {
			hash_free(c->facts);
			globfree(&scripts);
			return -1;
		}
	}

	globfree(&scripts);
	return 0;
}

int client_get_policy(client *c)
{
	if (pdu_send_GET_POLICY(&c->session, c->facts) < 0) {
		CRITICAL("Unable to GET_POLICY");
		exit(1);
	}

	pdu_receive(&c->session);
	if (RECV_PDU(&c->session)->op != PROTOCOL_OP_SEND_POLICY) {
		CRITICAL("Unexpected op from server: %u", RECV_PDU(&c->session)->op);

		client_disconnect(c);
		exit(1);
	}

	if (pdu_decode_SEND_POLICY(RECV_PDU(&c->session), &c->policy) != 0) {
		CRITICAL("Unable to decode SEND_POLICY PDU");

		client_disconnect(c);
		exit(1);
	}

	return 0;
}

int client_get_file(protocol_session *session, sha1 *checksum, int fd)
{
	size_t bytes = 0, n = 0;

	if (pdu_send_GET_FILE(session, checksum) != 0) {
		fprintf(stderr, "FAILED GET_FILE %s\n", checksum->hex);
		return -1;
	}

	do {
		pdu_receive(session);
		if (RECV_PDU(session)->op != PROTOCOL_OP_FILE_DATA) {
			pdu_send_ERROR(session, 600, "Invalid operation");
			return -1;
		}

		n = pdu_decode_FILE_DATA(RECV_PDU(session), fd);
		bytes += n;
	} while(n > 0);

	return bytes;
}

int client_enforce_policy(client *c, struct list *l)
{
	struct pwdb *passwd;
	struct spdb *shadow;
	struct grdb *group;
	struct sgdb *gshadow;

	struct res_user *ru;
	struct res_group *rg;
	struct res_file *rf;

	struct report *r;
	int pipefd[2];
	ssize_t bytes = 0;

	if ((passwd  = pwdb_init(SYS_PASSWD)) == NULL
	 || (shadow  = spdb_init(SYS_SHADOW)) == NULL
	 || (group   = grdb_init(SYS_GROUP))  == NULL
	 || (gshadow = sgdb_init(SYS_GSHADOW)) == NULL) {
		CRITICAL("Unable to initialize user / group database(s)");
		exit(2);
	}

	/* Remediate groups */
	for_each_node(rg, &c->policy->res_groups, res) {
		res_group_stat(rg, group, gshadow);
		r = res_group_remediate(rg, c->dryrun, group, gshadow);
		list_add_tail(&r->rep, l);
	}

	/* Remediate users */
	for_each_node(ru, &c->policy->res_users, res) {
		res_user_stat(ru, passwd, shadow);
		r = res_user_remediate(ru, c->dryrun, passwd, shadow);
		list_add_tail(&r->rep, l);
	}

	/* Remediate files */
	for_each_node(rf, &c->policy->res_files, res) {
		rf->rf_uid = pwdb_lookup_uid(passwd, rf->rf_owner);
		rf->rf_gid = grdb_lookup_gid(group,  rf->rf_group);

		res_file_stat(rf);

		if (res_file_different(rf, SHA1)) {
			if (pipe(pipefd) != 0) {
				pipefd[0] = -1;
			} else {
				bytes = client_get_file(&c->session, &rf->rf_rsha1, pipefd[1]);
				if (bytes <= 0) {
					close(pipefd[0]);
					close(pipefd[1]);

					pipefd[0] = -1;
				}
			}
		}
		r = res_file_remediate(rf, c->dryrun, pipefd[0], bytes);

		close(pipefd[0]);
		close(pipefd[1]);

		list_add_tail(&r->rep, l);
	}

	if (!c->dryrun) {
		pwdb_write(passwd,  SYS_PASSWD);
		spdb_write(shadow,  SYS_SHADOW);
		grdb_write(group,   SYS_GROUP);
		sgdb_write(gshadow, SYS_GSHADOW);
	}

	return 0;
}

int client_print_report(FILE *io, struct list *report)
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

/**************************************************************/

static int gather_facts(const char *script, struct hash *facts)
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
