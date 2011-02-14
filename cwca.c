#include "clockwork.h"

#include <glob.h>
#include <getopt.h>

#include "server.h"
#include "cert.h"


/* NOTES

  Invocation:

  cwca new            - Generate a self-signed rootca cert
  cwca issued         - Show issued certs
  cwca pending        - List pending requests
 *cwca revoked        - List revoked certificates
  cwca [fqdn]         - Details about [fqdn]'s cert/req
  cwca details [fqdn] - Details about [fqdn]'s cert/req
  cwca sign [fqdn]    - Sign [fqdn]'s req
  cwca ignore [fqdn]  - Ignore (and remove) [fqdn]'s req
  cwca revoke [fqdn]  - Revoke signed certificate

  */

/**************************************************************/

#define CWCA_SUCCESS   0
#define CWCA_SSL_ERROR 1
#define CWCA_OTHER_ERR 2

struct cwca_opts {
	char *command;
	char *fqdn;
	server *server;
};

/**************************************************************/

static struct cwca_opts* cwca_options(int argc, char **argv);

/* Command runners */
static int cwca_help_main(const struct cwca_opts *args);
static int cwca_new_main(const struct cwca_opts *args);
static int cwca_issued_main(const struct cwca_opts *args);
static int cwca_pending_main(const struct cwca_opts *args);
static int cwca_details_main(const struct cwca_opts *args);
static int cwca_sign_main(const struct cwca_opts *args);
static int cwca_ignore_main(const struct cwca_opts *args);
static int cwca_revoke_main(const struct cwca_opts *args);

/**************************************************************/

int main(int argc, char **argv)
{
	int err;
	struct cwca_opts *args;

	cert_init();

	args = cwca_options(argc, argv);
	if (strcmp(args->command, "new") == 0) {
		err = cwca_new_main(args);
	} else if (strcmp(args->command, "issued") == 0) {
		err = cwca_issued_main(args);
	} else if (strcmp(args->command, "pending") == 0) {
		err = cwca_pending_main(args);
	} else if (strcmp(args->command, "sign") == 0) {
		err = cwca_sign_main(args);
	} else if (strcmp(args->command, "help") == 0) {
		return cwca_help_main(args);
	} else if (strcmp(args->command, "details") == 0) {
		err = cwca_details_main(args);
	} else if (strcmp(args->command, "ignore") == 0) {
		err = cwca_ignore_main(args);
	} else if (strcmp(args->command, "revoke") == 0) {
		err = cwca_revoke_main(args);
	} else {
		fprintf(stderr, "Unknown command '%s'\n", args->command);
		fprintf(stderr, "Try 'cwca help' for a list of known commands.\n");
	}

	if (err != CWCA_SSL_ERROR) {
		return err;
	}

	ERR_print_errors_fp(stderr);
	return 1;
}

/**************************************************************/

struct cwca_opts* cwca_options(int argc, char **argv)
{
	struct cwca_opts *cwca;

	const char *short_opts = "h?c:";
	struct option long_opts[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "config", required_argument, NULL, 'c' },
		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;

	cwca = xmalloc(sizeof(struct cwca_opts));
	cwca->command = strdup("pending");
	cwca->fqdn    = NULL;
	cwca->server  = xmalloc(sizeof(server));

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1 ) {
		switch (opt) {
		case 'c':
			free(cwca->server->config_file);
			cwca->server->config_file = strdup(optarg);
			break;
		case 'h':
		case '?':
		default:
			free(cwca->command);
			cwca->command = strdup("help");
		}
	}

	if (optind < argc) {
		free(cwca->command);
		cwca->command = strdup(argv[optind++]);
	}

	if (optind < argc) {
		free(cwca->fqdn);
		cwca->fqdn = strdup(argv[optind++]);
	}

	if (server_options(cwca->server) != 0) {
		fprintf(stderr, "Unable to process server options");
		exit(2);
	}

	return cwca;
}

static int cwca_help_main(const struct cwca_opts *args)
{
	printf("cwca - Clockwork Policy Master Certificate Authority Tool\n"
	       "USAGE: cwca [COMMAND] [ARGS]\n"
	       "\n"
	       "Valid commands are:\n"
	       "\n"
	       "  new     - Setup a new CA, with a self-signed certificate.\n"
	       "  issued  - List signed certificates, by host name and fingerprint.\n"
	       "  pending - List pending signing requests, by host name and fingerprint.\n"
	       "  details - Print information about a named host's certificate / request.\n"
	       "  sign    - Sign a named host's certificate request.\n"
	       "  ignore  - Ignore (and remove) a host's certificate request.\n"
	       "  revoke  - Revoke a host's issued certificate.\n"
	       "  help    - Show this message.\n"
	       "\n"
	       "If COMMAND is passed as the fully qualified domain name of a host,\n"
	       "cwca will assume that 'details' was specified for the command, and\n"
	       "the host name is the first argument.  (i.e. cwca details [fqdn])\n"
	       "\n"
	       "If no COMMAND is given, cwca will default to 'pending'.\n"
	       "\n"
	       "ARGS can be any or all of the following:\n"
	       "\n"
	       "  -h, --help            Show this helpful message.\n"
	       "                        (for more in-depth help, check the man pages.)\n"
	       "\n"
	       "  -c, --config          Specify the path to an alternate configuration file.\n"
	       "\n");

	return CWCA_SUCCESS;
}

static int cwca_new_main(const struct cwca_opts *args)
{
	/* Stuff to ask the user for:

	   lifetime of root ca cert
	   lifetime of signed certs
	 */

	/* for now, some sane defaults... */
	int days   = 365 * 10;

	X509_REQ *request;
	X509 *cert;
	EVP_PKEY *key;

	/* FIXME: confirm new ca setup with user... */
	/* FIXME: remove previous private key */

	printf("Generating private/public keypair.\n");
	key = cert_retrieve_key(args->server->key_file);
	if (!key) { return CWCA_SSL_ERROR; }

	printf("Generating self-signed root CA certificate.\n");
	request = cert_generate_request(key, "Clockwork Policy Master");
	if (!request) { return CWCA_SSL_ERROR; }

	cert = cert_sign_request(request, key, days);
	if (!cert) { return CWCA_SSL_ERROR; }

	if (cert_store_certificate(cert, args->server->cert_file) != 0) {
		perror("Unable to store CA certificate");
		return CWCA_OTHER_ERR;
	}

	/* FIXME: set up CA directory structure? */
	printf("Complete.\n");
	return CWCA_SUCCESS;
}

static int cwca_issued_main(const struct cwca_opts *args)
{
	// iterate through #{args->server->certs_dir}/*.pem
	glob_t certs;
	size_t i;
	char *cert_files;
	X509 *cert;
	int rc;

	cert_files = string("%s/*.pem", args->server->certs_dir);
	rc = glob(cert_files, GLOB_MARK, NULL, &certs);
	free(cert_files);

	switch (rc) {
	case GLOB_NOMATCH:
		printf("No certificates have been issued.\n");
		return CWCA_SUCCESS;

	case GLOB_NOSPACE:
	case GLOB_ABORTED:
		fprintf(stderr, "couldn't list issued certificates.\n");
		return CWCA_OTHER_ERR;
	}

	for (i = 0; i < certs.gl_pathc; i++) {
		cert = cert_retrieve_certificate(certs.gl_pathv[i]);
		if (!cert) {
			printf("%s does not look like a PEM-encoded certificate.\n",
			       certs.gl_pathv[i]);
			continue;
		}

		printf("%s\t%s\n",
		       cert_certificate_subject_name(cert),
		       cert_fingerprint_certificate(cert));
	}
	globfree(&certs);
	return CWCA_SUCCESS;
}

static int cwca_pending_main(const struct cwca_opts *args)
{
	// iterate through #{args->server->requests_dir}/*.csr
	glob_t requests;
	size_t i;
	char *req_files;
	int rc;
	X509_REQ *req;
	EVP_PKEY *pubkey;

	req_files = string("%s/*.csr", args->server->requests_dir);
	rc = glob(req_files, GLOB_MARK, NULL, &requests);
	free(req_files);

	switch (rc) {
	case GLOB_NOMATCH:
		printf("No certificate signing requests pending.\n");
		printf(" (in %s)\n", args->server->requests_dir);
		return CWCA_SUCCESS;

	case GLOB_NOSPACE:
	case GLOB_ABORTED:
		fprintf(stderr, "couldn't list pending certificate signing requests.\n");
		return CWCA_OTHER_ERR;
	}

	for (i = 0; i < requests.gl_pathc; i++) {
		req = cert_retrieve_request(requests.gl_pathv[i]);
		if (!req) {
			fprintf(stderr, "%s does not appear to be a Certificate Request.\n", requests.gl_pathv[i]);
			continue;
		}

		pubkey = X509_REQ_get_pubkey(req);
		if (!pubkey) {
			fprintf(stderr, "%s could not be verified.\n", requests.gl_pathv[i]);
			continue;
		}

		printf("%s\t(%s)\n", cert_request_subject_name(req), requests.gl_pathv[i]);
	}
	globfree(&requests);
	return CWCA_SUCCESS;
}

static int cwca_details_main(const struct cwca_opts *args)
{
	EVP_PKEY *key;
	X509_REQ *request;
	X509 *cert;
	char *req_file;
	char *cert_file;

	if (!args->fqdn) {
		fprintf(stderr, "Invalid invocation.\n"
		                "USAGE: cwca details [fqdn]\n");
		return CWCA_OTHER_ERR;
	}

	req_file  = string("%s/%s.csr", args->server->requests_dir, args->fqdn);
	cert_file = string("%s/%s.pem", args->server->certs_dir,    args->fqdn);
	if (!req_file || !cert_file) { return CWCA_OTHER_ERR; }

	key = cert_retrieve_key(args->server->key_file);
	if (!key) { return CWCA_SSL_ERROR; }

	request = cert_retrieve_request(req_file);
	cert = cert_retrieve_certificate(cert_file);

	if (cert) {
		printf("Found a certificate\n at %s\n\n", cert_file);
		X509_print_fp(stdout, cert);
	} else if (request) {
		printf("Found a certificate signing request\n at %s\n\n", req_file);
		X509_REQ_print_fp(stdout, request);
	}

	return CWCA_SUCCESS;
}

static int cwca_sign_main(const struct cwca_opts *args)
{
	EVP_PKEY *key;
	X509_REQ *request;
	X509 *cert;
	char *req_file;
	char *cert_file;

	if (!args->fqdn) {
		fprintf(stderr, "Invalid invocation.\n"
		                "USAGE: cwca sign [fqdn]\n");
		return CWCA_OTHER_ERR;
	}

	req_file  = string("%s/%s.csr", args->server->requests_dir, args->fqdn);
	cert_file = string("%s/%s.pem", args->server->certs_dir,    args->fqdn);
	if (!req_file || !cert_file) { return CWCA_OTHER_ERR; }

	printf("Retrieving private/public keypair.\n");
	key = cert_retrieve_key(args->server->key_file);
	if (!key) { return CWCA_SSL_ERROR; }

	printf("Loading certificate signing request from %s\n", req_file);
	request = cert_retrieve_request(req_file);
	if (!request) { return CWCA_SSL_ERROR; }

	printf("Signing certificate\n");
	cert = cert_sign_request(request, key, 365*10);
	if (!cert) { return CWCA_SSL_ERROR; }

	printf("Storing signed certifcate in %s\n", cert_file);
	if (cert_store_certificate(cert, cert_file) != 0) {
		perror("Unable to store certificate");
		return CWCA_OTHER_ERR;
	}

	printf("Removing certificate signing request.\n");
	unlink(req_file);

	return CWCA_SUCCESS;
}

static int cwca_ignore_main(const struct cwca_opts *args)
{
	char *req_file;

	if (!args->fqdn) {
		fprintf(stderr, "Invalid invocation.\n"
		                "USAGE: cwca ignore [fqdn]\n");
		return CWCA_OTHER_ERR;
	}

	req_file = string("%s/%s.csr", args->server->requests_dir, args->fqdn);

	printf("Removing certificate signing request for %s\n", args->fqdn);
	unlink(req_file);

	return CWCA_SUCCESS;
}

static int cwca_revoke_main(const struct cwca_opts *args)
{

	if (!args->fqdn) {
		fprintf(stderr, "Invalid invocation.\n"
		                "USAGE: cwca revoke [fqdn]\n");
		return CWCA_OTHER_ERR;
	}

	fprintf(stderr, "'revoke' not yet implemented.\n");
	return CWCA_SUCCESS;
}

