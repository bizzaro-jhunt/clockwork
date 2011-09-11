/*
  Copyright 2011 James Hunt <james@jameshunt.us>

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
};

int server_options(struct server *args);

#endif
