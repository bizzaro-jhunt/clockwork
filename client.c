#include "client.h"
#include "cert.h"
#include "conf/parser.h"

/**************************************************************/

static client default_options = {
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

static client* configured_options(const char *path);
static int merge_clients(client *a, client *b);

/**************************************************************/

static client* configured_options(const char *path)
{
	client *c;
	struct hash *config;
	char *v;

	c = xmalloc(sizeof(client));

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
static int merge_clients(client *a, client *b)
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

int client_options(client *args)
{
	client *cfg;
	cfg = configured_options(args->config_file ? args->config_file : default_options.config_file);

	if (merge_clients(args, cfg) != 0
	 || merge_clients(args, &default_options) != 0) {
		return 1;
	}
	args->log_level += (cfg->log_level == 0 ? default_options.log_level : cfg->log_level);

	return 0;
}

int client_connect(client *c)
{
	char *addr;
	long err;

	protocol_ssl_init();

	INFO("Setting up client SSL context");
	if (!(c->ssl_ctx = SSL_CTX_new(TLSv1_method()))) {
		ERROR("Failed to set up new TLSv1 SSL context");
		protocol_ssl_backtrace();
		return -1;
	}

	DEBUG(" - Loading CA certificate chain from %s", c->ca_cert_file);
	if (!SSL_CTX_load_verify_locations(c->ssl_ctx, c->ca_cert_file, NULL)) {
		ERROR("Failed to load CA certificate chain (%s)", c->ca_cert_file);
		SSL_CTX_free(c->ssl_ctx); c->ssl_ctx = NULL;
		protocol_ssl_backtrace();
		return -1;
	}

	DEBUG(" - Loading certificate from %s", c->cert_file);
	if (!SSL_CTX_use_certificate_file(c->ssl_ctx, c->cert_file, SSL_FILETYPE_PEM)) {
		WARNING("No certificate to load (%s); running in 'unverified client' mode");
	}

	DEBUG(" - Loading private key from %s", c->key_file);
	if (!SSL_CTX_use_PrivateKey_file(c->ssl_ctx, c->key_file, SSL_FILETYPE_PEM)) {
		ERROR("Failed to load private key (%s)", c->key_file);
		SSL_CTX_free(c->ssl_ctx); c->ssl_ctx = NULL;
		protocol_ssl_backtrace();
		return -1;
	}

	DEBUG(" - Setting peer verification flags");
	SSL_CTX_set_verify(c->ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	SSL_CTX_set_verify_depth(c->ssl_ctx, 4);

	addr = string("%s:%s", c->s_address, c->s_port);
	DEBUG("Connecting to %s", addr);

	c->socket = BIO_new_connect(addr);
	if (!c->socket || BIO_do_connect(c->socket) <= 0) {
		DEBUG("Failed to create a new BIO connection object");
		goto failed;
	}

	if (!(c->ssl = SSL_new(c->ssl_ctx))) {
		DEBUG("Failed to create an SSL connection");
		goto failed;
	}

	SSL_set_bio(c->ssl, c->socket, c->socket);
	if (SSL_connect(c->ssl) <= 0) {
		DEBUG("Failed to initiate SSL connection");
		goto failed;
	}

	if ((err = protocol_ssl_verify_peer(c->ssl, c->s_address)) != X509_V_OK) {
		CRITICAL("Server certificate verification failed: %s", X509_verify_cert_error_string(err));
		protocol_ssl_backtrace();
		return -1;
	}

	protocol_session_init(&c->session, c->ssl);

	return 0;

failed:
	CRITICAL("Connection failed");
	protocol_ssl_backtrace();
	return -1;
}

int client_hello(client *c)
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

int client_bye(client *c)
{
	pdu_send_BYE(&c->session);
	client_disconnect(c);
	return 0;
}

int client_disconnect(client *c)
{
	protocol_session_deinit(&c->session);

	SSL_shutdown(c->ssl);
	SSL_free(c->ssl);
	c->ssl = NULL;

	SSL_CTX_free(c->ssl_ctx);
	c->ssl_ctx = NULL;

	return 0;
}
