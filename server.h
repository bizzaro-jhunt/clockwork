#ifndef _SERVER_H
#define _SERVER_H

#include "clockwork.h"
#include "proto.h"
#include "threads.h"

#define DEFAULT_CONFIG_FILE   "/etc/clockwork/policyd.conf"
#define DEFAULT_MANIFEST_FILE "/etc/clockwork/manifest.pol"

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
	char *cert_file;
	char *key_file;

	char *port;
} server;

int server_init(server *s);
int server_deinit(server *s);

int server_loop(server *s);

void* server_worker_thread(void *ssl);
void* server_manager_thread(void*);

#endif
