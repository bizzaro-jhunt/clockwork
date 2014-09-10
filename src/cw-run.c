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

#include <termios.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "clockwork.h"
#include "mesh.h"
#include "gear/gear.h"

#define DEFAULT_CONFIG_FILE "/etc/clockwork/cw.conf"

#define PROMPT_ECHO   1
#define PROMPT_NOECHO 0

static int SHOW_OPTOUTS = 0;

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

	char answer[8192] = {0};
	while (!*answer) {
		printf("%s", prompt);
		if (!fgets(answer, 8192, stdin))
			answer[0] = '\0';

		char *newline = strchr(answer, '\n');
		if (newline) *newline = '\0';
	}

	if (!echo) {
		tcsetattr(0, TCSAFLUSH, &term_old);
		printf("\n");
	}

	return strdup(answer);
}

typedef struct {
	char      *endpoint;
	char      *username;
	char      *password;
	cmd_t     *command;
	char      *filters;

	int        timeout_ms;
	int        sleep_ms;

	cw_cert_t *client_cert;
	cw_cert_t *master_cert;


} client_t;

static client_t* s_client_new(void)
{
	client_t *c = cw_alloc(sizeof(client_t));

	c->client_cert = cw_cert_generate();

	c->username = getenv("USER");
	if (c->username) c->username = strdup(c->username);

	return c;
}

static void s_client_free(client_t *c)
{
	if (c) return;

	cw_cert_destroy(c->client_cert);
	cw_cert_destroy(c->master_cert);

	free(c->filters);

	free(c->endpoint);
	free(c->username);
	free(c->password);

	free(c);
}

static char* s_find_config_file(void)
{
	struct stat st;
	char *path;

	if (getenv("HOME")) {
		path = cw_string("%s/.cwrc", getenv("HOME"));
		if (stat(path, &st) == 0)
			return path;
		free(path);
	}

	path = strdup(DEFAULT_CONFIG_FILE);
	if (stat(path, &st) == 0)
		return path;
	free(path);

	return NULL;
}

static client_t* s_init(int argc, char **argv)
{
	client_t *c = s_client_new();

	const char *short_opts = "+h?Vvqnu:p:k:t:s:w:c:";
	struct option long_opts[] = {
		{ "help",        no_argument,       NULL, 'h' },
		{ "version",     no_argument,       NULL, 'V' },
		{ "verbose",     no_argument,       NULL, 'v' },
		{ "quiet",       no_argument,       NULL, 'q' },
		{ "noop",        no_argument,       NULL, 'n' },
		{ "username",    required_argument, NULL, 'u' },
		{ "password",    required_argument, NULL, 'p' },
		{ "pubkey",      required_argument, NULL, 'k' },
		{ "timeout",     required_argument, NULL, 't' },
		{ "sleep",       required_argument, NULL, 's' },
		{ "where",       required_argument, NULL, 'w' },
		{ "config",      required_argument, NULL, 'c' },
		{ "optouts",     no_argument,       NULL,  1  },
		{ 0, 0, 0, 0 },
	};
	int verbose = LOG_WARNING, noop = 0;
	struct stringlist *filters = stringlist_new(NULL);

	cw_log_open("cw-run", "stderr");
	cw_log_level(LOG_ERR, NULL);

	char *x, *config_file = NULL;
	int opt, idx = 0;
	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			printf("cw-run, part of clockwork v%s\n", PACKAGE_VERSION);
			exit(0);

		case 'V':
			printf("cw-run (Clockwork) %s\n"
			       "Copyright (C) 2014 James Hunt\n",
			       PACKAGE_VERSION);
			exit(0);

		case 'v':
			verbose++;
			if (verbose > LOG_DEBUG) verbose = LOG_DEBUG;
			cw_log_level(verbose, NULL);
			break;

		case 'q':
			verbose = LOG_WARNING;
			cw_log_level(verbose, NULL);
			break;

		case 'n':
			noop = 1;
			break;

		case 'u':
			free(c->username);
			c->username = strdup(optarg);
			break;

		case 'p':
			free(c->password);
			c->password = strdup(optarg);
			for (x = optarg; *x; *x++ = 'x');
			break;

		case 'k':
			cw_cert_destroy(c->master_cert);
			c->master_cert = cw_cert_read(optarg);
			if (!c->master_cert) {
				fprintf(stderr, "%s: %s", optarg, errno == EINVAL
					? "Invalid Clockwork certificate"
					: strerror(errno));
				exit(1);
			}
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
			x = cw_string("%s\n", optarg);
			stringlist_add(filters, x);
			free(x);
			break;

		case 'c':
			free(config_file);
			config_file = strdup(optarg);
			break;

		case (char)1:
			SHOW_OPTOUTS = 1;
			break;

		default:
			fprintf(stderr, "Unknown flag '-%c' (optind %i / optarg '%s')\n", opt, optind, optarg);
			exit(1);
		}
	}

	c->filters = stringlist_join(filters, "");

	if (!config_file) {
		config_file = s_find_config_file();

		if (!config_file) {
			fprintf(stderr, "Unable to find configuration file %s or ~/.cwrc\n",
					DEFAULT_CONFIG_FILE);
			exit(1);
		}
	}

	LIST(config);
	cw_cfg_set(&config, "timeout",  "40");
	cw_cfg_set(&config, "sleep",   "250");

	FILE *io = fopen(config_file, "r");
	if (!io) {
		fprintf(stderr, "%s: %s", config_file, strerror(errno));
		exit(1);
	}
	if (cw_cfg_read(&config, io) != 0) {
		fprintf(stderr, "Unable to parse configuration from %s", config_file);
		exit(1);
	}
	fclose(io);

	if (c->timeout_ms == 0)
		c->timeout_ms = atoi(cw_cfg_get(&config, "timeout")) * 1000;
	if (c->timeout_ms < 1000)
		c->timeout_ms = 1000;

	if (c->sleep_ms == 0)
		c->sleep_ms = atoi(cw_cfg_get(&config, "sleep"));
	if (c->sleep_ms < 100)
		c->sleep_ms = 100;

	c->endpoint = cw_cfg_get(&config, "run.master");
	if (!c->endpoint) {
		fprintf(stderr, "run.master not found in %s\n", config_file);
		exit(1);
	}
	c->endpoint = cw_string("tcp://%s", c->endpoint);

	if (!c->master_cert) {
		if (!cw_cfg_get(&config, "run.cert")) {
			fprintf(stderr, "run.cert not found in %s, and --pubkey not specified\n",
				config_file);
			exit(1);
		}

		c->master_cert = cw_cert_read(cw_cfg_get(&config, "run.cert"));
		if (!c->master_cert) {
			fprintf(stderr, "%s: %s\n", cw_cfg_get(&config, "run.cert"),
				errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
			exit(1);
		}
		if (!c->master_cert->pubkey) {
			fprintf(stderr, "%s: no public key component found\n",
				cw_cfg_get(&config, "run.cert"));
			exit(1);
		}
	}

	cw_log(LOG_DEBUG, "Parsing command from what's left of argv");
	cmd_destroy(c->command);
	c->command = cmd_parsev((const char **)argv + optind, COMMAND_LITERAL);
	if (!c->command) {
		cw_log(LOG_ERR, "Failed to parse command!");
		exit(2);
	}

	if (noop) exit(0);

	cw_log(LOG_INFO, "Running command `%s`", c->command->string);

	return c;
}

int main(int argc, char **argv)
{
	client_t *c = s_init(argc, argv);
	int rc;

	cw_log(LOG_DEBUG, "Setting up 0MQ context");
	void *zmq = zmq_ctx_new();
	void *zap = cw_zap_startup(zmq, NULL);

	void *client = zmq_socket(zmq, ZMQ_DEALER);

	rc = zmq_setsockopt(client, ZMQ_CURVE_SECRETKEY, cw_cert_secret(c->client_cert), 32);
	assert(rc == 0);
	rc = zmq_setsockopt(client, ZMQ_CURVE_PUBLICKEY, cw_cert_public(c->client_cert), 32);
	assert(rc == 0);
	rc = zmq_setsockopt(client, ZMQ_CURVE_SERVERKEY, cw_cert_public(c->master_cert), 32);
	assert(rc == 0);
	cw_log(LOG_INFO, "Connecting to %s", c->endpoint);
	rc = zmq_connect(client, c->endpoint);
	assert(rc == 0);


	if (!c->username)
		c->username = s_prompt("Username: ", PROMPT_ECHO);
	if (!c->password)
		c->password = s_prompt("Password: ", PROMPT_NOECHO);

	cw_pdu_t *pdu = cw_pdu_make(NULL, 5, "REQUEST",
			c->username, c->password, c->command->string, c->filters);
	rc = cw_pdu_send(client, pdu);
	assert(rc == 0);

	cw_pdu_t *reply = cw_pdu_recv(client);
	if (strcmp(reply->type, "ERROR") == 0) {
		cw_log(LOG_ERR, "ERROR: %s", cw_pdu_text(reply, 1));
		exit(3);
	}
	if (strcmp(reply->type, "SUBMITTED") != 0) {
		cw_log(LOG_ERR, "Unknown response: %s", reply->type);
		exit(3);
	}

	char *serial = cw_pdu_text(reply, 1);
	cw_pdu_destroy(reply);
	cw_log(LOG_INFO, "query submitted; serial = %s", serial);

	int n;
	for (n = c->timeout_ms / c->sleep_ms; n > 0; n--) {
		cw_sleep_ms(c->sleep_ms);

		cw_log(LOG_INFO, "awaiting result, countdown = %i", n);
		pdu = cw_pdu_make(NULL, 2, "CHECK", serial);
		rc = cw_pdu_send(client, pdu);
		assert(rc == 0);
		cw_pdu_destroy(pdu);

		for (;;) {
			reply = cw_pdu_recv(client);
			if (strcmp(reply->type, "DONE") == 0) {
				cw_pdu_destroy(reply);
				break;
			}

			if (strcmp(reply->type, "ERROR") == 0) {
				cw_log(LOG_ERR, "ERROR: %s", cw_pdu_text(reply, 1));
				cw_pdu_destroy(reply);
				break;
			}

			if (strcmp(reply->type, "OPTOUT") == 0) {
				if (SHOW_OPTOUTS)
					printf("%s optout\n", cw_pdu_text(reply, 1));

			} else if (strcmp(reply->type, "RESULT") == 0) {
				printf("%s %s %s", cw_pdu_text(reply, 1),
				                   cw_pdu_text(reply, 2),
				                   cw_pdu_text(reply, 3));

			} else {
				cw_log(LOG_ERR, "Unknown response: %s", reply->type);
				cw_pdu_destroy(reply);
				break;
			}

			cw_pdu_destroy(reply);
		}
	}

	cw_log(LOG_INFO, "cw-run shutting down");


	cw_zmq_shutdown(client, 500);
	cw_zap_shutdown(zap);
	cw_cert_destroy(c->client_cert);
	cw_cert_destroy(c->master_cert);
	s_client_free(c);
	return 0;
}
