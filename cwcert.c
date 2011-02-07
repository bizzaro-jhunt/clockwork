#include "clockwork.h"
#include "cert.h"

#include <getopt.h>

/* FIXME: how do we get config into this? */

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

#define DEFAULT_KEY_FILE     "/etc/clockwork/ssl/agent/key.pem"
#define DEFAULT_REQUEST_FILE "/etc/clockwork/ssl/agent/csr.pem"
#define DEFAULT_CERT_FILE    "/etc/clockwork/ssl/agent/cert.pem"

struct cwcert_opts {
	char *command;

	char *key_file;
	char *req_file;
	char *cert_file;
};

struct cwcert_opts* cwcert_get_options(int argc, char **argv);

#define CWCERT_SUCCESS   0
#define CWCERT_SSL_ERROR 1
#define CWCERT_OTHER_ERR 2

int cwcert_help_main(const struct cwcert_opts *opts);
int cwcert_details_main(const struct cwcert_opts *opts);
int cwcert_new_main(const struct cwcert_opts *opts);
int cwcert_renew_main(const struct cwcert_opts *opts);

int main(int argc, char **argv)
{
	int err;
	struct cwcert_opts *opts;

	cert_init();

	opts = cwcert_get_options(argc, argv);

	if (argc < 2) {
		return cwcert_help_main(opts);
	}

	if (strcmp(opts->command, "details") == 0) {
		err = cwcert_details_main(opts);
	} else if (strcmp(opts->command, "new") == 0) {
		err = cwcert_new_main(opts);
	} else if (strcmp(opts->command, "renew") == 0) {
		err = cwcert_renew_main(opts);
	} else {
		return cwcert_help_main(opts);
	}

	if (err != CWCERT_SSL_ERROR) {
		return err;
	}

	ERR_print_errors_fp(stderr);
	return 1;
}

struct cwcert_opts* cwcert_get_options(int argc, char **argv)
{
	struct cwcert_opts *opts;
	const char *short_opts = "h?c:r:k:";
	struct option long_opts[] = {
		{ "cert",    required_argument, NULL, 'c' },
		{ "request", required_argument, NULL, 'r' },
		{ "key",     required_argument, NULL, 'k' },
		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;

	opts = xmalloc(sizeof(struct cwcert_opts));
	opts->command      = NULL;
	opts->key_file     = strdup(DEFAULT_KEY_FILE);
	opts->req_file     = strdup(DEFAULT_REQUEST_FILE);
	opts->cert_file    = strdup(DEFAULT_CERT_FILE);

	if (argc < 2) { return opts; }
	opts->command = strdup(argv[1]);

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'c':
			free(opts->cert_file);
			opts->cert_file = strdup(optarg);
			break;
		case 'r':
			free(opts->req_file);
			opts->req_file = strdup(optarg);
			break;
		case 'k':
			free(opts->key_file);
			opts->key_file = strdup(optarg);
			break;
		case 'h':
		case '?':
		default:
			free(opts->command);
			opts->command = strdup("help");
		}
	}

	return opts;
}

int cwcert_help_main(const struct cwcert_opts *opts)
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
	       "  --cert /path/to/cert-file\n"
	       "  -c     /path/to/cert-file\n"
	       "      Specify the path to the certificate file for this agent.\n"
	       "\n"
	       "  --request /path/to/request-file\n"
	       "  -r        /path/to/request-file\n"
	       "      Specify the path the certificate signing request file.\n"
	       "\n"
	       "  --key /path/to/key-file\n"
	       "  -k    /path/to/key-file\n"
	       "      Specify the path to this agent's private/public keypair.\n"
	       "\n"
	       "  -h\n"
	       "  -?\n"
	       "      Show this message.\n"
	       "\n");
	return CWCERT_SUCCESS;
}

int cwcert_details_main(const struct cwcert_opts *opts)
{
	X509_REQ *request;
	X509 *cert;

	cert = cert_retrieve_certificate(opts->cert_file);
	request = cert_retrieve_request(opts->req_file);

	if (cert) {
		printf("Found a certificate\n at %s\n\n", opts->cert_file);
		X509_print_fp(stdout, cert);
	} else if (request) {
		printf("Found a certificate signing request\n at %s\n\n", opts->req_file);
		X509_REQ_print_fp(stdout, request);
	} else {
		fprintf(stderr,
		        "No certificate or signing request found at:\n"
		        "  certificate: %s\n"
		        "  signing request: %s\n",
		        opts->cert_file, opts->req_file);

		return CWCERT_OTHER_ERR;
	}

	return CWCERT_SUCCESS;
}

int cwcert_new_main(const struct cwcert_opts *opts)
{
	EVP_PKEY *key;
	X509_REQ *request;

	printf("Using private/public keypair in %s\n", opts->key_file);
	key = cert_retrieve_key(opts->key_file);
	if (!key) {
		fprintf(stderr, "Unable to generate or retrieve private/public key.\n");
		return CWCERT_SSL_ERROR;
	}

	printf("Generating certificate signing request...\n");
	request = cert_generate_request(key, "Clockwork Agent");
	if (!request) {
		fprintf(stderr, "Unable to generate certificate signing request.\n");
		return CWCERT_SSL_ERROR;
	}

	printf("Saving certificate signing request to %s\n", opts->req_file);
	if (cert_store_request(request, opts->req_file) != 0) {
		perror("Unable to save certificate signing request" );
		return CWCERT_OTHER_ERR;
	}

	printf("Completed.\n");
	return CWCERT_SUCCESS;
}

int cwcert_renew_main(const struct cwcert_opts *opts)
{
	fprintf(stderr, "'renew' not yet implemented...\n");
	return 1;
}

