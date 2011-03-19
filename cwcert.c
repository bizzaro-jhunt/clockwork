#include "clockwork.h"

#include <getopt.h>

#include "cert.h"
#include "client.h"
#include "prompt.h"

/* NOTES

  Invocation:

  cwcert         - (variable) Invoke 'new' if we don't have a cert/req
                              Invoke 'details' if we do

  cwcert details - Show details about my cert/req
  cwcert new     - Generate a new cert request
  cwcert renew   - Renew my certificate
  cwcert test    - Test certificate against policy master

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

static int negotiate_certificate(client *c, X509_REQ *csr);

static int cwcert_help_main(const struct cwcert_opts *args);
static int cwcert_details_main(const struct cwcert_opts *args);
static int cwcert_new_main(const struct cwcert_opts *args);
static int cwcert_renew_main(const struct cwcert_opts *args);
static int cwcert_test_main(const struct cwcert_opts *args);

int main(int argc, char **argv)
{
	int err;
	struct cwcert_opts *args;

	cert_init();

	args = cwcert_options(argc, argv);
	args->config->log_level = log_level(args->config->log_level);
	INFO("Log level is now %s (%u)",
	     log_level_name(args->config->log_level),
	     args->config->log_level);

	if (strcmp(args->command, "details") == 0) {
		err = cwcert_details_main(args);
	} else if (strcmp(args->command, "new") == 0) {
		err = cwcert_new_main(args);
	} else if (strcmp(args->command, "renew") == 0) {
		err = cwcert_renew_main(args);
	} else if (strcmp(args->command, "test") == 0) {
		err = cwcert_test_main(args);
	} else {
		return cwcert_help_main(args);
	}

	if (err != CWCERT_SSL_ERROR) {
		return err;
	}

	fprintf(stderr, "OpenSSL error stack\n"
	                "-------------------\n");
	ERR_print_errors_fp(stderr);
	return 1;
}

struct cwcert_opts* cwcert_options(int argc, char **argv)
{
	struct cwcert_opts *args;

	const char *short_opts = "h?c:Dv";
	struct option long_opts[] = {
		{ "config",  required_argument, NULL, 'c' },
		{ "debug",   no_argument,       NULL, 'D' },
		{ "verbose", no_argument,       NULL, 'v' },
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
		case 'D':
			args->config->log_level = LOG_LEVEL_DEBUG;
			break;
		case 'v':
			args->config->log_level++;
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

static int negotiate_certificate(client *c, X509_REQ *csr)
{
	X509 *cert = NULL;

	if (pdu_send_GET_CERT(&c->session, csr) < 0) { exit(1); }
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
	       "  test    - Test local certificate against policy master.\n"
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
	       "\n"
	       "  -v, --verbose         Increase verbosity / log level by one.\n"
	       "                        (can be used more than once, i.e. -vvv)\n"
	       "\n"
	       "  -D, --debug           Set verbosity to DEBUG level.  Mainly intended for\n"
	       "                        developers, but helpful in troubleshooting.\n"
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
	struct cert_subject subject;
	char fqdn[1024];
	char *confirmation = NULL;

	key = cert_retrieve_key(args->config->key_file);
	if (!key) {
		INFO("Creating new key");
		key = cert_generate_key(2048);
		if (!key) {
			CRITICAL("Unable to create new key");
			return CWCERT_SSL_ERROR;
		}
		if (cert_store_key(key, args->config->key_file) != 0) {
			CRITICAL("Unable to store new key in %s", args->config->key_file);
			return CWCERT_SSL_ERROR;
		}
	} else {
		CRITICAL("Unable to retrieve key from %s", args->config->key_file);
		return CWCERT_SSL_ERROR;
	}

	if (client_connect(args->config) != 0) { exit(1); }
	if (client_hello(args->config) == 0) {
		printf("Valid certificate already acquired.\n"
		       "Use `cwca revoke' on policy master to revoke.\n");
		client_bye(args->config);
		exit(0);
	}

	memset(&subject, 0, sizeof(subject));
	subject.type = strdup("CWA");
	if (cert_my_hostname(fqdn, 1024) != 0) {
		ERROR("Failed to get local hostname!");
		return CWCERT_OTHER_ERR;
	}
	subject.fqdn = fqdn;

	do {
		printf("You will now be asked for server identity information.\n"
		       "\n"
		       "The answers you provide will be used to construct the subject name\n"
		       "of the certificate signing request sent to the Clockwork policy master.\n"
		       "\n");

		cert_prompt_for_subject(&subject);

		printf("\nGenerating new certificate for:\n\n");
		cert_print_subject(stdout, "  ", &subject);
		printf("\nSubject for host certificate will be:\n\n  ");
		cert_print_subject_terse(stdout, &subject);
		printf("\n\n");

		do {
			free(confirmation);
			confirmation = prompt_with_echo("Is this information correct (yes or no) ? ");
		} while (strcmp(confirmation, "yes") != 0 && strcmp(confirmation, "no") != 0);

	} while (strcmp(confirmation, "yes") != 0);

	INFO("Generating certificate signing request...");
	request = cert_generate_request(key, &subject);
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
	while (negotiate_certificate(args->config, request) != 0) {
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

static int cwcert_test_main(const struct cwcert_opts *args)
{
	EVP_PKEY *key;

	key = cert_retrieve_key(args->config->key_file);
	if (!key) {
		ERROR("No private key found.  Have you run `cwcert new'?");
		return CWCERT_OTHER_ERR;
	}

	if (client_connect(args->config) != 0) {
		ERROR("Connection to %s:%s refused", args->config->s_address, args->config->s_port);
		return CWCERT_OTHER_ERR;
	}
	if (client_hello(args->config) == 0) {
		printf("Certificate accepted by policy master\n");
		client_bye(args->config);
		return CWCERT_SUCCESS;
	}

	printf("Certificate DENIED by policy master\n");
	return CWCERT_OTHER_ERR;
}

