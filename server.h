#ifndef _SERVER_H
#define _SERVER_H

#include "clockwork.h"
#include "proto.h"
#include "threads.h"

#define SERVER_OPT_UNSPEC  0
#define SERVER_OPT_TRUE    1
#define SERVER_OPT_FALSE  -1

/**
  A listening Server process' state
 */
typedef struct {
	/** OpenSSL I/O object for the bound socket. */
	BIO *listener;
	/** OpenSSL context for setting up new SSL connections. */
	SSL_CTX *ssl_ctx;

	/** Whether or not to emit debugging information (option) */
	int debug;
	/** Requested log verbosity level (option) */
	int log_level;

	/** Path to the policy master configuration file. */
	char *config_file;
	/** Path to the root policy manifest file. */
	char *manifest_file;

	/** Whether or not to fork off as a daemon (option) */
	int daemonize;
	/** Path to the daemon's lock file (option) */
	char *lock_file;
	/** Path to the daemon's PID file (option) */
	char *pid_file;

	/** Path to the Certificate Authority certificate */
	char *ca_cert_file;
	/** Path to the Certificate Revocation List */
	char *crl_file;
	/** Path to this node's certificate */
	char *cert_file;
	/** Path to this node's private key */
	char *key_file;

	/** Path to the policy master reporting database file */
	char *db_file;

	/** Directory in which to store/find client certificate signing requests. */
	char *requests_dir;
	/** Directory in which to store/find client certificates. */
	char *certs_dir;

	/** Directory to cache data from the clients (facts, etc.) */
	char *cache_dir;

	/** TCP port to listen on for incoming client connections. */
	char *port;

	/** Whether or not to invoke the --show-config behavior. */
	int show_config;
} server;

int server_options(server *args);

#endif
