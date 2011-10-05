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

#include "client.h"
#include "cert.h"
#include "conf/parser.h"

/**************************************************************/

static struct client default_options = {
	.log_level   = LOG_LEVEL_ERROR,
	.mode        = 0,

	.config_file  = "/etc/clockwork/cwa.conf",

	.ca_cert_file = "/etc/clockwork/ssl/CA.pem",
	.cert_file    = "/etc/clockwork/ssl/cert.pem",
	.request_file = "/etc/clockwork/ssl/request.pem",
	.key_file     = "/etc/clockwork/ssl/key.pem",

	.db_file      = "/var/lib/clockwork/agent.db",

	.gatherers = "/etc/clockwork/gather.d/*",

	.s_address = "clockwork",
	.s_port    = "7890"
};

/**************************************************************/

static struct client* configured_options(const char *path);
static int merge_clients(struct client *a, struct client *b);

/**************************************************************/

static struct client* configured_options(const char *path)
{
	struct client *c;
	struct hash *config;
	char *v;

	c = xmalloc(sizeof(struct client));

	config = parse_config(path);
	if (config) {
		v = hash_get(config, "ca_cert_file");
		if (v) { c->ca_cert_file = strdup(v); }

		v = hash_get(config, "cert_file");
		if (v) { c->cert_file = strdup(v); }

		v = hash_get(config, "request_file");
		if (v) { c->request_file = strdup(v); }

		v = hash_get(config, "key_file");
		if (v) { c->key_file = strdup(v); }

		v = hash_get(config, "db_file");
		if (v) { c->db_file = strdup(v); }

		v = hash_get(config, "gatherers");
		if (v) { c->gatherers = strdup(v); }

		v = hash_get(config, "server");
		if (v) { c->s_address = strdup(v); }

		v = hash_get(config, "port");
		if (v) { c->s_port = strdup(v); }

		v = hash_get(config, "log_level");
		if (v) {
			if (strcmp(v, "critical") == 0) {
				c->log_level = LOG_LEVEL_CRITICAL;
			} else if (strcmp(v, "error") == 0) {
				c->log_level = LOG_LEVEL_ERROR;
			} else if (strcmp(v, "warning") == 0) {
				c->log_level = LOG_LEVEL_WARNING;
			} else if (strcmp(v, "notice") == 0) {
				c->log_level = LOG_LEVEL_NOTICE;
			} else if (strcmp(v, "info") == 0) {
				c->log_level = LOG_LEVEL_INFO;
			} else if (strcmp(v, "debug") == 0) {
				c->log_level = LOG_LEVEL_DEBUG;
			} else if (strcmp(v, "all") == 0) {
				c->log_level = LOG_LEVEL_ALL;
			} else { // handle "none" implicitly
				c->log_level = LOG_LEVEL_NONE;
			}
		}
	}

	return c;
}

#define MERGE_STRING_OPTION(a,b,opt) do {\
	if (!a->opt && b->opt) {\
		a->opt = strdup(b->opt);\
	}\
} while(0)
static int merge_clients(struct client *a, struct client *b)
{
	MERGE_STRING_OPTION(a,b,config_file);
	MERGE_STRING_OPTION(a,b,ca_cert_file);
	MERGE_STRING_OPTION(a,b,cert_file);
	MERGE_STRING_OPTION(a,b,request_file);
	MERGE_STRING_OPTION(a,b,key_file);
	MERGE_STRING_OPTION(a,b,db_file);
	MERGE_STRING_OPTION(a,b,gatherers);
	MERGE_STRING_OPTION(a,b,s_address);
	MERGE_STRING_OPTION(a,b,s_port);
	if (a->mode == 0) { a->mode = b->mode; }

	return 0;
}
#undef MERGE_STRING_OPTION

/**************************************************************/

int client_options(struct client *args)
{
	struct client *cfg;
	cfg = configured_options(args->config_file ? args->config_file : default_options.config_file);

	if (merge_clients(args, cfg) != 0
	 || merge_clients(args, &default_options) != 0) {
		return 1;
	}
	args->log_level += (cfg->log_level == 0 ? default_options.log_level : cfg->log_level);

	return 0;
}

static SSL_CTX* client_ssl_ctx(struct client *c, int use_cert)
{
	SSL_CTX *ctx;

	INFO("Setting up client SSL context");
	if (!(ctx = SSL_CTX_new(TLSv1_method()))) {
		ERROR("Failed to set up new TLSv1 SSL context");
		protocol_ssl_backtrace();
		return NULL;
	}

	DEBUG(" - Loading CA certificate chain from %s", c->ca_cert_file);
	if (!SSL_CTX_load_verify_locations(ctx, c->ca_cert_file, NULL)) {
		ERROR("Failed to load CA certificate chain (%s)", c->ca_cert_file);
		SSL_CTX_free(ctx);
		protocol_ssl_backtrace();
		return NULL;
	}

	c->verified = 0;
	if (use_cert) {
		DEBUG(" - Loading certificate from %s", c->cert_file);
		if (!SSL_CTX_use_certificate_file(ctx, c->cert_file, SSL_FILETYPE_PEM)) {
			WARNING("No certificate to load (%s); running in 'unverified client' mode");
		} else {
			c->verified = 1;
		}
	}

	DEBUG(" - Loading private key from %s", c->key_file);
	if (!SSL_CTX_use_PrivateKey_file(ctx, c->key_file, SSL_FILETYPE_PEM)) {
		ERROR("Failed to load private key (%s)", c->key_file);
		SSL_CTX_free(ctx);
		protocol_ssl_backtrace();
		return NULL;
	}

	DEBUG(" - Setting peer verification flags");
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	SSL_CTX_set_verify_depth(ctx, 4);

	return ctx;
}

static int client_do_connect(struct client *c)
{
	char *addr;

	addr = string("%s:%s", c->s_address, c->s_port);
	DEBUG("Connecting to %s", addr);

	c->socket = BIO_new_connect(addr);
	if (!c->socket || BIO_do_connect(c->socket) <= 0) {
		DEBUG("Failed to create a new BIO connection object");
		return -1;
	}

	if (!(c->ssl = SSL_new(c->ssl_ctx))) {
		DEBUG("Failed to create an SSL connection");
		return -1;
	}

	SSL_set_bio(c->ssl, c->socket, c->socket);
	if (SSL_connect(c->ssl) <= 0) {
		DEBUG("Failed to initiate SSL connection");
		protocol_ssl_backtrace();
		SSL_free(c->ssl);
		c->ssl = NULL;
		return -1;
	}

	return 0;
}

int client_connect(struct client *c, int unverified)
{
	long err;
	int connected = 0;

	protocol_ssl_init();

	c->ssl_ctx = client_ssl_ctx(c, 1);
	connected = client_do_connect(c);
	if (connected != 0 && c->verified && unverified) {
		DEBUG("Verified mode failed; switching to unverified mode");
		SSL_CTX_free(c->ssl_ctx);
		c->ssl_ctx = client_ssl_ctx(c, 0);
		connected = client_do_connect(c);
	}

	if (connected != 0) {
		DEBUG("SSL connection (%sverified mode) failed", unverified ? "un" : "");
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

int client_hello(struct client *c)
{
	if (pdu_send_HELLO(&c->session) < 0) { exit(1); }

	if (pdu_receive(&c->session) < 0) {
		if (c->session.errnum == 401) {
			DEBUG("Peer certificate required");
			return -1;
		}
		CRITICAL("Error: %u - %s", c->session.errnum, c->session.errstr);
		client_disconnect(c);
		exit(1);
	}

	if (RECV_PDU(&c->session)->op != PROTOCOL_OP_HELLO) {
		pdu_send_ERROR(&c->session, 505, "Protocol Error");
		client_disconnect(c);
		exit(1);
	}

	return 0;
}

int client_bye(struct client *c)
{
	pdu_send_BYE(&c->session);
	client_disconnect(c);
	return 0;
}

int client_disconnect(struct client *c)
{
	protocol_session_deinit(&c->session);

	SSL_shutdown(c->ssl);
	SSL_free(c->ssl);
	c->ssl = NULL;

	SSL_CTX_free(c->ssl_ctx);
	c->ssl_ctx = NULL;

	return 0;
}
