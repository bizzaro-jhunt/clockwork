#ifndef _SERVER_H
#define _SERVER_H

#include "clockwork.h"
#include "proto.h"
#include "threads.h"

#define SERVER_OPT_UNSPEC  0
#define SERVER_OPT_TRUE    1
#define SERVER_OPT_FALSE  -1

typedef struct {
	BIO *listener;
	SSL_CTX *ssl_ctx;

	int debug;
	int log_level;

	char *config_file;
	char *manifest_file;

	int daemonize;
	char *lock_file;
	char *pid_file;

	char *ca_cert_file;
	char *crl_file;
	char *cert_file;
	char *key_file;

	char *requests_dir;
	char *certs_dir;

	char *port;
} server;

int server_options(server *args);

#endif
