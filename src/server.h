/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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

#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>

#define THREAD_TYPE                pthread_t
#define THREAD_CREATE(id,ent,arg)  pthread_create(&(id), NULL, (ent), (arg))

#include "clockwork.h"
#include "proto.h"

#define SERVER_OPT_UNSPEC  0
#define SERVER_OPT_TRUE    1
#define SERVER_OPT_FALSE  -1

/**
  A listening Server process' state
 */
struct server {
	BIO *listener;         /* bound SSL socket */
	SSL_CTX *ssl_ctx;      /* OpenSSL context for new connections */

	int debug;             /* emit debugging? (option) */
	int show_config;       /* invoke the --show-config behavior? */
	int test_config;       /* invoke the --test behavior? */
	int log_level;         /* log level (option) */

	char *config_file;     /* path to policy master config file */
	char *manifest_file;   /* path to policy manifest file */

	int   daemonize;       /* to fork, or not to fork */
	char *lock_file;       /* path to daemon's lock file (option) */
	char *pid_file;        /* path to daemon's PID file (option) */

	char *ca_cert_file;    /* path to CA certificate */
	char *crl_file;        /* path to certificate revocation list */
	char *cert_file;       /* path to policy master's certificate */
	char *key_file;        /* path to policy master's private key */

	char *requests_dir;    /* where to store signing requests */
	char *certs_dir;       /* where to store signed certificates */

	char *cache_dir;       /* where to cache client data (i.e. facts) */

	char *listen;          /* interface/port to listen on */

	int autosign;          /* whether or not to automatically sign certs */

	int retain_days;       /* How many days of reports to keep */
};

int server_options(struct server *args);

#endif
