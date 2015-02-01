/*
  Copyright 2011-2015 James Hunt <james@jameshunt.us>

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

#include <termios.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sodium.h>

#include "../src/clockwork.h"
#include "../src/mesh.h"

#define PROMPT_ECHO   1
#define PROMPT_NOECHO 0

#define FORMAT_YAML   1
#define FORMAT_JSON   2
#define FORMAT_XML    3
#define FORMAT_TEXT   4
#define FORMAT_PRETTY 5

static int SHOW_OPTOUTS = 0;
static int FORMAT = FORMAT_PRETTY;

static char* s_prompt(const char *prompt, int echo)
{
	struct termios term_new, term_old;
	if (!echo) {
		if (tcgetattr(0, &term_old) != 0)
			return NULL;
		term_new = term_old;
		term_new.c_lflag &= ~ECHO;

		if (tcsetattr(0, TCSAFLUSH, &term_new) != 0)
			return NULL;
	}

	int attempts = 0;
	char answer[8192] = {0};
	while (!*answer) {
		if (attempts++ > 1) fprintf(stderr, "\n");
		fprintf(stderr, "%s", prompt);
		if (!fgets(answer, 8192, stdin))
			answer[0] = '\0';

		char *newline = strchr(answer, '\n');
		if (newline) *newline = '\0';
	}

	if (!echo) {
		tcsetattr(0, TCSAFLUSH, &term_old);
		fprintf(stderr, "\n");
	}

	return strdup(answer);
}

typedef struct {
	char      *endpoint;
	char      *username;
	char      *password;
	char      *authkey;
	cmd_t     *command;
	char      *filters;

	int        timeout_ms;
	int        sleep_ms;

	cert_t    *client_cert;
	cert_t    *master_cert;
	cert_t    *auth_cert;
} client_t;

static client_t* s_client_new(void)
{
	client_t *c = vmalloc(sizeof(client_t));

	c->client_cert = cert_generate(VIGOR_CERT_ENCRYPTION);

	c->username = getenv("USER");
	if (c->username) c->username = strdup(c->username);

	if (getenv("HOME")) {
		c->authkey = string("%s/.clockwork/mesh.key", getenv("HOME"));

		struct stat st;
		if (stat(c->authkey, &st) != 0) {
			logger(LOG_INFO, "Default key %s not found; skipping implicit keyauth", c->authkey);
			free(c->authkey);
			c->authkey = NULL;
		}
	}

	return c;
}

static void s_client_free(client_t *c)
{
	if (c) return;

	cert_free(c->client_cert);
	cert_free(c->master_cert);

	free(c->filters);

	free(c->endpoint);
	free(c->username);
	free(c->password);

	free(c);
}

static int s_read_config(list_t *cfg, const char *file, int must_exist)
{
	assert(file);
	assert(cfg);

	struct stat st;
	if (stat(file, &st) != 0) {
		if (must_exist) {
			fprintf(stderr, "%s: %s\n", file, strerror(ENOENT));
			return 1;
		}
		return 0;
	}

	FILE *io = fopen(file, "r");
	if (!io) {
		fprintf(stderr, "%s: %s\n", file, strerror(errno));
		return 1;
	}

	if (config_read(cfg, io) != 0) {
		fprintf(stderr, "Unable to grok %s\n", file);
		fclose(io);
		return 1;
	}

	fclose(io);
	return 0;
}

static client_t* s_init(int argc, char **argv)
{
	client_t *c = s_client_new();

	const char *short_opts = "+h?Vvqnu:k:t:s:w:c:YJXTP";
	struct option long_opts[] = {
		{ "help",        no_argument,       NULL, 'h' },
		{ "version",     no_argument,       NULL, 'V' },
		{ "verbose",     no_argument,       NULL, 'v' },
		{ "quiet",       no_argument,       NULL, 'q' },
		{ "noop",        no_argument,       NULL, 'n' },
		{ "username",    required_argument, NULL, 'u' },
		{ "key",         required_argument, NULL, 'k' },
		{ "timeout",     required_argument, NULL, 't' },
		{ "sleep",       required_argument, NULL, 's' },
		{ "where",       required_argument, NULL, 'w' },
		{ "config",      required_argument, NULL, 'c' },
		{ "optouts",     no_argument,       NULL,  1  },
		{ "cert",        required_argument, NULL,  2  },

		{ "yaml",        no_argument,       NULL, 'Y' },
		{ "json",        no_argument,       NULL, 'J' },
		{ "xml",         no_argument,       NULL, 'X' },
		{ "text",        no_argument,       NULL, 'T' },
		{ "pretty",      no_argument,       NULL, 'P' },
		{ 0, 0, 0, 0 },
	};
	int verbose = LOG_WARNING, noop = 0;
	strings_t *filters = strings_new(NULL);

	log_open("cw-mesh", "stderr");
	log_level(LOG_ERR, NULL);

	char *x, *config_file = NULL;
	int opt, idx = 0;
	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			printf("cw-mesh, part of clockwork v%s\n", PACKAGE_VERSION);
			exit(0);

		case 'V':
			printf("cw-mesh (Clockwork) %s\n"
			       "Copyright (C) 2014 James Hunt\n",
			       PACKAGE_VERSION);
			exit(0);

		case 'v':
			verbose++;
			if (verbose > LOG_DEBUG) verbose = LOG_DEBUG;
			log_level(verbose, NULL);
			break;

		case 'q':
			verbose = LOG_WARNING;
			log_level(verbose, NULL);
			break;

		case 'n':
			noop = 1;
			break;

		case 'u':
			free(c->username);
			c->username = strdup(optarg);
			break;

		case 'k':
			free(c->authkey);
			c->authkey = strcmp(optarg, "0") == 0 ? NULL : strdup(optarg);
			break;

		case 't':
			c->timeout_ms = atoi(optarg) * 1000;
			if (c->timeout_ms <=    0) c->timeout_ms = 40 * 1000;
			if (c->timeout_ms <  1000) c->timeout_ms =  1 * 1000;
			break;

		case 's':
			c->sleep_ms = atoi(optarg);
			if (c->sleep_ms <=   0) c->sleep_ms = 250;
			if (c->sleep_ms <  100) c->sleep_ms = 100;
			break;

		case 'w':
			x = string("%s\n", optarg);
			strings_add(filters, x);
			free(x);
			break;

		case 'c':
			free(config_file);
			config_file = strdup(optarg);
			break;

		case 1:
			SHOW_OPTOUTS = 1;
			break;

		case 2:
			cert_free(c->master_cert);
			c->master_cert = cert_read(optarg);
			if (!c->master_cert) {
				fprintf(stderr, "%s: %s", optarg, errno == EINVAL
					? "Invalid Clockwork certificate"
					: strerror(errno));
				exit(1);
			}
			break;

		case 'Y': FORMAT = FORMAT_YAML;   break;
		case 'J': FORMAT = FORMAT_JSON;   break;
		case 'X': FORMAT = FORMAT_XML;    break;
		case 'T': FORMAT = FORMAT_TEXT;   break;
		case 'P': FORMAT = FORMAT_PRETTY; break;

		default:
			fprintf(stderr, "Unknown flag '-%c' (optind %i / optarg '%s')\n", opt, optind, optarg);
			exit(1);
		}
	}

	log_level(verbose, NULL);
	c->filters = strings_join(filters, "");

	LIST(config);
	config_set(&config, "mesh.timeout",  "40");
	config_set(&config, "mesh.sleep",   "250");

	if (!config_file) {
		if (s_read_config(&config, CW_MTOOL_CONFIG_FILE, 0) != 0)
			exit(1);

		if (getenv("HOME")) {
			char *path = string("%s/.cwrc", getenv("HOME"));
			if (s_read_config(&config, path, 0) != 0)
				exit(1);

			config_file = string("%s or %s", CW_MTOOL_CONFIG_FILE, path);
			free(path);

		} else {
			config_file = strdup(CW_MTOOL_CONFIG_FILE);
		}

	} else if (s_read_config(&config, config_file, 1) != 0) {
		exit(1);
	}

	if (c->timeout_ms == 0)
		c->timeout_ms = atoi(config_get(&config, "mesh.timeout")) * 1000;
	if (c->timeout_ms < 1000)
		c->timeout_ms = 1000;

	if (c->sleep_ms == 0)
		c->sleep_ms = atoi(config_get(&config, "mesh.sleep"));
	if (c->sleep_ms < 100)
		c->sleep_ms = 100;

	c->endpoint = config_get(&config, "mesh.master");
	if (!c->endpoint) {
		fprintf(stderr, "mesh.master not found in %s\n", config_file);
		exit(1);
	}
	c->endpoint = string("tcp://%s", c->endpoint);

	if (!c->master_cert) {
		if (!config_get(&config, "mesh.cert")) {
			fprintf(stderr, "mesh.cert not found in %s, and --cert not specified\n",
				config_file);
			exit(1);
		}

		c->master_cert = cert_read(config_get(&config, "mesh.cert"));
		if (!c->master_cert) {
			fprintf(stderr, "%s: %s\n", config_get(&config, "mesh.cert"),
				errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
			exit(1);
		}
		if (!c->master_cert->pubkey) {
			fprintf(stderr, "%s: no public key component found\n",
				config_get(&config, "mesh.cert"));
			exit(1);
		}
	}

	if (c->authkey) {
		c->auth_cert = cert_read(c->authkey);
		if (!c->auth_cert) {
			fprintf(stderr, "%s: %s\n", c->authkey,
				errno == EINVAL ? "Invalid Clockwork key" : strerror(errno));
			exit(1);
		}
		if (c->auth_cert->type != VIGOR_CERT_SIGNING) {
			fprintf(stderr, "%s: Incorrect key/certificate type (not a %%sig v1 key)\n", c->authkey);
			exit(1);
		}
		if (!c->auth_cert->seckey) {
			fprintf(stderr, "%s: no secret key component found\n", c->authkey);
			exit(1);
		}
	}

	logger(LOG_DEBUG, "Parsing command from what's left of argv");
	cmd_destroy(c->command);
	c->command = cmd_parsev((const char **)argv + optind, COMMAND_LITERAL);
	if (!c->command) {
		logger(LOG_ERR, "Failed to parse command!");
		exit(2);
	}

	if (noop) exit(0);

	logger(LOG_INFO, "Running command `%s`", c->command->string);

	return c;
}

void print_header(void)
{
	switch (FORMAT) {
	case FORMAT_YAML:
		printf("---\n");
		break;

	case FORMAT_JSON:
		printf("{\n");
		break;

	case FORMAT_XML:
		printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		printf("<results>\n");
		break;
	}
}

void print_optout(const char *fqdn, const char *uuid)
{
	switch (FORMAT) {
	case FORMAT_YAML:
		printf("%s:\n", uuid);
		printf("  fqdn: %s\n", fqdn);
		printf("  status: ~\n");
		printf("  output: ~\n");
		break;

	case FORMAT_JSON:
		printf("  \"%s\": {\n", uuid);
		printf("    \"fqdn\"   : \"%s\",\n", fqdn);
		printf("    \"status\" : null,\n");
		printf("    \"output\" : null\n");
		printf("  },\n");
		break;

	case FORMAT_XML:
		printf("  <result uuid=\"%s\">\n", uuid);
		printf("    <fqdn>%s</fqdn>\n", fqdn);
		printf("    <optout/>\n");
		printf("  </result>\n");
		break;

	case FORMAT_TEXT:
		printf("%s %s optout\n", uuid, fqdn);
		break;

	case FORMAT_PRETTY:
		printf("%s opted out\nUUID %s\n\n", fqdn, uuid);
		break;
	}
}

static void printf_eachline(const char *fmt, const char *buf)
{
	strings_t *lines = strings_split(buf, strlen(buf), ":", SPLIT_NORMAL);
	int i;
	for (i = 0; i < lines->num; i++)
		printf(fmt, lines->strings[i]);
}

void print_result(const char *fqdn, const char *uuid, const char *result, const char *output)
{
	char *esc;
	switch (FORMAT) {
	case FORMAT_YAML:
		printf("%s:\n", uuid);
		printf("  fqdn: %s\n", fqdn);
		printf("  status: %s\n", result);
		printf("  output: |-\n");
		printf_eachline("    %s\n", output);
		break;

	case FORMAT_JSON:
		printf("  \"%s\": {\n", uuid);
		printf("    \"fqdn\"   : \"%s\",\n", fqdn);
		printf("    \"status\" : \"%s\",\n", result);
		esc = cw_escape_json(output);
		printf("    \"output\" : \"%s\"\n",  esc);
		free(esc);
		printf("  }\n");
		break;

	case FORMAT_XML:
		printf("  <result uuid=\"%s\">\n", uuid);
		printf("    <fqdn>%s</fqdn>\n", fqdn);
		printf("    <status>%s</status>\n", result);
		esc = cw_escape_cdata(output);
		printf("    <output><![CDATA[%s]]></output>\n", esc);
		free(esc);
		printf("  </result>\n");
		break;

	case FORMAT_TEXT:
		printf("%s %s %s %s\n", uuid, fqdn,
			strcmp(result, "0") == 0 ? "success" : "failed",
			result);
		printf_eachline(" %s\n", output);
		break;

	case FORMAT_PRETTY:
		if (strcmp(result, "0") == 0)
			printf("%s succeeded\n", fqdn);
		else
			printf("%s failed (rc=%s)\n", fqdn, result);
		printf("UUID %s\n", uuid);
		printf("------------------------------------------------------------\n");
		printf_eachline("  %s\n", output);
		printf("\n\n");
		break;
	}
}

void print_footer(void)
{
	switch (FORMAT) {
	case FORMAT_JSON:
		printf("}\n");
		break;

	case FORMAT_XML:
		printf("</results>\n");
		break;
	}
}

int main(int argc, char **argv)
{
	client_t *c = s_init(argc, argv);
	int rc;

	logger(LOG_DEBUG, "Setting up 0MQ context");
	void *zmq = zmq_ctx_new();
	void *zap = zap_startup(zmq, NULL);

	void *client = zmq_socket(zmq, ZMQ_DEALER);

	rc = zmq_setsockopt(client, ZMQ_CURVE_SECRETKEY, cert_secret(c->client_cert), 32);
	if (rc != 0) {
		logger(LOG_CRIT, "Failed to set ZMQ_CURVE_SECRETKEY option on client socket: %s",
			zmq_strerror(errno));
		if (errno == EINVAL) {
			int maj, min, rev;
			zmq_version(&maj, &min, &rev);
			logger(LOG_CRIT, "Perhaps the local libzmq (%u.%u.%u) doesn't support CURVE security?",
				maj, min, rev);
		}
		exit(4);
	}
	rc = zmq_setsockopt(client, ZMQ_CURVE_PUBLICKEY, cert_public(c->client_cert), 32);
	assert(rc == 0);
	rc = zmq_setsockopt(client, ZMQ_CURVE_SERVERKEY, cert_public(c->master_cert), 32);
	assert(rc == 0);
	logger(LOG_INFO, "Connecting to %s", c->endpoint);
	rc = zmq_connect(client, c->endpoint);
	assert(rc == 0);


	if (!c->username)
		c->username = s_prompt("Username: ", PROMPT_ECHO);
	if (!c->auth_cert)
		c->password = s_prompt("Password: ", PROMPT_NOECHO);

	pdu_t *pdu = pdu_make("REQUEST", 1, c->username);
	if (c->auth_cert) {
		assert(c->auth_cert);

		unsigned char unsealed[256 - 64];
		randombytes(unsealed, 256 - 64);

		uint8_t *sealed;
		unsigned long long slen;
		slen = cert_seal(c->auth_cert, unsealed, 256 - 64, &sealed);
		assert(slen == 256);

		char *pubkey = cert_public_s(c->auth_cert);
		pdu_extendf(pdu, "%s", pubkey);
		pdu_extend( pdu, sealed, 256);

		free(pubkey);
		free(sealed);
	} else {
		assert(c->password);
		pdu_extendf(pdu, "");
		pdu_extendf(pdu, "%s", c->password);
	}
	pdu_extendf(pdu, "%s", c->command->string);
	pdu_extendf(pdu, "%s", c->filters);

	rc = pdu_send(pdu, client);
	assert(rc == 0);

	pdu_t *reply = pdu_recv(client);
	if (strcmp(pdu_type(reply), "ERROR") == 0) {
		logger(LOG_ERR, "ERROR: %s", pdu_string(reply, 1));
		exit(3);
	}
	if (strcmp(pdu_type(reply), "SUBMITTED") != 0) {
		logger(LOG_ERR, "Unknown response: %s", pdu_type(reply));
		exit(3);
	}

	char *serial = pdu_string(reply, 1);
	pdu_free(reply);
	logger(LOG_INFO, "query submitted; serial = %s", serial);

	print_header();

	int n;
	for (n = c->timeout_ms / c->sleep_ms; n > 0; n--) {
		sleep_ms(c->sleep_ms);

		logger(LOG_INFO, "awaiting result, countdown = %i", n);
		pdu = pdu_make("CHECK", 1, serial);
		rc = pdu_send_and_free(pdu, client);
		assert(rc == 0);

		for (;;) {
			reply = pdu_recv(client);
			if (strcmp(pdu_type(reply), "DONE") == 0) {
				pdu_free(reply);
				break;
			}

			if (strcmp(pdu_type(reply), "ERROR") == 0) {
				logger(LOG_ERR, "ERROR: %s", pdu_string(reply, 1));
				pdu_free(reply);
				break;
			}


			if (strcmp(pdu_type(reply), "OPTOUT") == 0) {
				if (SHOW_OPTOUTS) {
					char *fqdn = pdu_string(reply, 1);
					char *uuid = pdu_string(reply, 2);
					print_optout(fqdn, uuid);

					free(fqdn);
					free(uuid);
				}

			} else if (strcmp(pdu_type(reply), "RESULT") == 0) {
				char *fqdn   = pdu_string(reply, 1);
				char *uuid   = pdu_string(reply, 2);
				char *result = pdu_string(reply, 3);
				char *output = pdu_string(reply, 4);

				print_result(fqdn, uuid, result, output);

				free(fqdn);
				free(uuid);
				free(result);
				free(output);

			} else {
				logger(LOG_ERR, "Unknown response: %s", pdu_type(reply));
				pdu_free(reply);
				break;
			}

			pdu_free(reply);
		}
	}

	print_footer();

	logger(LOG_INFO, "cw-mesh shutting down");


	vzmq_shutdown(client, 500);
	zap_shutdown(zap);
	cert_free(c->client_cert);
	cert_free(c->master_cert);
	s_client_free(c);
	return 0;
}
