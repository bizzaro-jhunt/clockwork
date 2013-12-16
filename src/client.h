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

#ifndef CLIENT_H
#define CLIENT_H

#include "clockwork.h"
#include "proto.h"
#include "policy.h"

#define CLIENT_MODE_ONLINE  1   /* connect to policy master and enforce policy */
#define CLIENT_MODE_OFFLINE 2   /* don't connect to policy master */
#define CLIENT_MODE_FACTS   3   /* generate facts to stdout */
#define CLIENT_MODE_TEST    4   /* connect to policy master but don't change anything */

/**
  A Client-side Interaction
 */
struct client {
	BIO *socket;               /* IO stream for policy master */
	SSL *ssl;                  /* SSL connection parameters */
	SSL_CTX *ssl_ctx;
	int verified;

	struct session session;  /* protocol session state machine */
	struct hash *facts;        /* local facts */
	struct policy *policy;     /* policy recieved from policy master */

	int mode;                  /* mode of operation; what client app does */
	int log_level;             /* how much output do you want? */
	char *config_file;         /* path to configuration file */

	char *ca_cert_file;        /* path to CA certificate */
	char *cert_file;           /* path to local certificate */
	char *request_file;        /* path to local certificate request */
	char *key_file;            /* path to local private key */

	char *db_file;             /* path to the local agent database */

	char *gatherers;           /* shell glob of fact gatherer scripts */

	char *s_address;           /* address of policy master */
	char *s_port;              /* TCP port of policy master */

	int retain_days;           /* How many days of reports to keep */
};

int client_options(struct client *args);

int client_connect(struct client *c, int unverified);
int client_hello(struct client *c);
int client_bye(struct client *c);
int client_disconnect(struct client *c);

#endif
