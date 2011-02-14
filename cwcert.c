#include "clockwork.h"

#include <getopt.h>

#include "cert.h"
#include "client.h"

/* NOTES

  Invocation:

  cwcert         - (variable) Invoke 'new' if we don't have a cert/req
                              Invoke 'details' if we do

  cwcert details - Show details about my cert/req
  cwcert new     - Generate a new cert request
  cwcert renew   - Renew my certificate

  ('cwcert new' and 'cwcert renew' may be called from cwa via shell-out,
   or, we may roll up the new and renew main()'s as library functions and
   call them directly.)
 */

#define CWCERT_SUCCESS   0
#define CWCERT_SSL_ERROR 1
#define CWCERT_OTHER_ERR 2

struct cwcert_opts {
	char *command;
	client *config;
};

struct cwcert_opts* cwcert_options(int argc, char **argv);

static int negotiate_certificate(client *c);

static int cwcert_help_main(const struct cwcert_opts *args);
static int cwcert_details_main(const struct cwcert_opts *args);
static int cwcert_new_main(const struct cwcert_opts *args);
static int cwcert_renew_main(const struct cwcert_opts *args);

int main(int argc, char **argv)
{
	int err;
	struct cwcert_opts *args;

	cert_init();

	args = cwcert_options(argc, argv);
	log_level(args->config->log_level);

	if (strcmp(args->command, "details") == 0) {
		err = cwcert_details_main(args);
	} else if (strcmp(args->command, "new") == 0) {
		err = cwcert_new_main(args);
	} else if (strcmp(args->command, "renew") == 0) {
		err = cwcert_renew_main(args);
	} else {
		return cwcert_help_main(args);
	}

	if (err != CWCERT_SSL_ERROR) {
		return err;
	}

	ERR_print_errors_fp(stderr);
	return 1;
}

struct cwcert_opts* cwcert_options(int argc, char **argv)
{
	struct cwcert_opts *args;

	const char *short_opts = "h?c:";
	struct option long_opts[] = {
		{ "config", required_argument, NULL, 'c' },
		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;

	args = xmalloc(sizeof(struct cwcert_opts));
	args->command = strdup("help");
	args->config  = xmalloc(sizeof(client));

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'c':
			free(args->config->config_file);
			args->config->config_file = strdup(optarg);
			break;
		case 'h':
		case '?':
		default:
			free(args->command);
			args->command = strdup("help");
		}
	}

	if (optind < argc) {
		free(args->command);
		args->command = strdup(argv[optind++]);
	}

	if (client_options(args->config) != 0) {
		ERROR("Unable to process client options");
		exit(2);
	}

	return args;
}

static int negotiate_certificate(client *c)
{
	X509_REQ *csr = NULL;
	X509 *cert = NULL;
	EVP_PKEY *pkey = NULL;

	csr = cert_retrieve_request(c->request_file);
	if (!csr) {
		pkey = cert_retrieve_key(c->key_file);
		if (!pkey) { goto failed; }

		csr = cert_generate_request(pkey, "Clockwork Agent");
		if (!csr) { goto failed; }

		cert_store_request(csr, c->request_file);
		EVP_PKEY_free(pkey);
		pkey = NULL;
	}

	if (pdu_send_GET_CERT(&c->session, csr) < 0) { exit(1); }

	X509_REQ_free(csr);
	csr = NULL;

	if (pdu_receive(&c->session) < 0) {
		CRITICAL("Error: %u - %s", c->session.errnum, c->session.errstr);
		client_disconnect(c);
		exit(1);
	}

	if (RECV_PDU(&c->session)->op != PROTOCOL_OP_SEND_CERT) {
		CRITICAL("Unexpected op from server: %u", RECV_PDU(&c->session)->op);
		client_disconnect(c);
		exit(1);
	}

	if (pdu_decode_SEND_CERT(RECV_PDU(&c->session), &cert) != 0) {
		client_disconnect(c);
		exit(1);
	}

	if (!cert) {
		return 1;
	}

	INFO("Received certificate from policy master");
	cert_store_certificate(cert, c->cert_file);

	X509_free(cert);
	cert = NULL;
	return 0;

failed:
	X509_REQ_free(csr);
	X509_free(cert);
	EVP_PKEY_free(pkey);
	return -1;
}

static int cwcert_help_main(const struct cwcert_opts *args)
{
	printf("cwcert - Clockwork Agent Certificate / Certificate Request Tool\n"
	       "USAGE: cwcert [COMMAND] [ARGS]\n"
	       "\n"
	       "Valid commands are:\n"
	       "\n"
	       "  details - Print information about this host's certificate / request.\n"
	       "  new     - Generate a new host certificate request.\n"
	       "  renew   - Renew this host's certificate / request.\n"
	       "  help    - Show this message.\n"
	       "\n"
	       "If no COMMAND is given, cwcert will pick either 'details' or 'new',\n"
	       "depending on whether or not it has a certificate / request.\n"
	       "\n"
	       "ARGS can be any or all of the following:\n"
	       "\n"
	       "  -h, --help            Show this helpful message.\n"
	       "                        (for more in-depth help, check the man pages.)\n"
	       "\n"
	       "  -c, --config          Specify the path to an alternate configuration file.\n"
	       "\n");
	return CWCERT_SUCCESS;
}

static int cwcert_details_main(const struct cwcert_opts *args)
{
	X509_REQ *request;
	X509 *cert;

	cert = cert_retrieve_certificate(args->config->cert_file);
	request = cert_retrieve_request(args->config->request_file);

	if (cert) {
		printf("Found a certificate\n at %s\n\n", args->config->cert_file);
		X509_print_fp(stdout, cert);
	} else if (request) {
		printf("Found a certificate signing request\n at %s\n\n", args->config->request_file);
		X509_REQ_print_fp(stdout, request);
	} else {
		fprintf(stderr,
		        "No certificate or signing request found at:\n"
		        "  certificate: %s\n"
		        "  signing request: %s\n",
		        args->config->cert_file, args->config->request_file);

		return CWCERT_OTHER_ERR;
	}

	return CWCERT_SUCCESS;
}

static int cwcert_new_main(const struct cwcert_opts *args)
{
	EVP_PKEY *key;
	X509_REQ *request;

	if (client_connect(args->config) != 0) {
		exit(1);
	}

	if (client_hello(args->config) == 0) {
		printf("Valid certificate already acquired.\n"
		       "Use `cwca revoke' on policy master to revoke.\n");
		client_bye(args->config);
		exit(0);
	}

	INFO("Using key in %s", args->config->key_file);
	key = cert_retrieve_key(args->config->key_file);
	if (!key) {
		CRITICAL("Unable to generate or retrieve private/public key.");
		return CWCERT_SSL_ERROR;
	}

	INFO("Generating certificate signing request...");
	request = cert_generate_request(key, "Clockwork Agent");
	if (!request) {
		CRITICAL("Unable to generate certificate signing request.");
		return CWCERT_SSL_ERROR;
	}

	INFO("Saving certificate signing request to %s", args->config->request_file);
	if (cert_store_request(request, args->config->request_file) != 0) {
		CRITICAL("Unable to save certificate signing request" );
		return CWCERT_OTHER_ERR;
	}

	INFO("Removing old certificate %s", args->config->cert_file);
	unlink(args->config->cert_file);

	INFO("Sending certificate signing request to server");
	while (negotiate_certificate(args->config) != 0) {
		printf("awaiting `cwca sign' on policy master...\n");
		sleep(5);
	}

	client_bye(args->config);
	if (client_connect(args->config) != 0) {
		CRITICAL("Unable to reconnect to verify");
		return CWCERT_OTHER_ERR;
	}

	if (client_hello(args->config) != 0) {
		CRITICAL("Issued certificate is not valid");
		client_bye(args->config);
		return CWCERT_OTHER_ERR;
	}

	INFO("Completed");
	client_bye(args->config);
	return CWCERT_SUCCESS;
}

static int cwcert_renew_main(const struct cwcert_opts *args)
{
	fprintf(stderr, "'renew' not yet implemented...\n");
	return 1;
}

