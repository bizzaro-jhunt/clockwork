#ifndef _CLIENT_H
#define _CLIENT_H

#include "clockwork.h"
#include "proto.h"
#include "policy.h"

#define DEFAULT_CONFIG_FILE "/etc/clockwork/cwa.conf"

typedef struct {
	BIO *socket;
	SSL *ssl;
	SSL_CTX *ssl_ctx;

	protocol_session session;
	struct hash *facts;
	struct policy *policy;

	int log_level;

	int dryrun;

	char *config_file;

	char *ca_cert_file;
	char *cert_file;
	char *key_file;

	char *gatherers;

	char *s_address;
	char *s_port;
} client;

int client_connect(client *c);
int client_disconnect(client *c);

int client_gather_facts(client *c);
int client_get_policy(client *c);
int client_get_file(protocol_session *session, sha1 *checksum, int fd);
int client_enforce_policy(client *c, struct list *report);
int client_print_report(FILE *io, struct list *report);

#endif
