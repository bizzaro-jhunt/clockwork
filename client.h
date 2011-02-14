#ifndef _CLIENT_H
#define _CLIENT_H

#include "clockwork.h"
#include "proto.h"
#include "policy.h"

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
	char *request_file;
	char *key_file;

	char *gatherers;

	char *s_address;
	char *s_port;
} client;

int client_options(client *args);

int client_connect(client *c);
int client_hello(client *c);
int client_bye(client *c);
int client_disconnect(client *c);

#endif
