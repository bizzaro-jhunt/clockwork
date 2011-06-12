#ifndef _CLIENT_H
#define _CLIENT_H

#include "clockwork.h"
#include "proto.h"
#include "policy.h"

/**
  A Client-side Interaction
 */
typedef struct {
	/** OpenSSL I/O object connected to the policy master. */
	BIO *socket;
	/** OpenSSL SSL object controlling the SSL connection. */
	SSL *ssl;
	/** OpenSSL context for setting up the SSL connection. */
	SSL_CTX *ssl_ctx;

	/** The protocol session state machine. */
	protocol_session session;
	/** Facts gathered for the local client. */
	struct hash *facts;
	/** Policy retrieved from the policy master for the local client. */
	struct policy *policy;

	/** Requested log verbosity level (option) */
	int log_level;

	/** Whether or not to actually apply changes required by policy. */
	int dryrun;

	/** Whether to run 'offline', without connecting to the policy master. */
	int offline;

	/** Path to the agent configuration file. */
	char *config_file;

	/** Path to the Certificate Authority certificate */
	char *ca_cert_file;
	/** Path to this node's certificate */
	char *cert_file;
	/** Path to this node's certificate signing request */
	char *request_file;
	/** Path to this node's private key */
	char *key_file;

	/** Path to the agent database file */
	char *db_file;

	/** Shell glob matching the fact gathering scripts to run. */
	char *gatherers;

	/** The IP/DNS address of the policy master (option). */
	char *s_address;
	/** TCP port to connect to on the policy master host (option). */
	char *s_port;
} client;

int client_options(client *args);

int client_connect(client *c);
int client_hello(client *c);
int client_bye(client *c);
int client_disconnect(client *c);

#endif
