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

#include "clockwork.h"

#include <glob.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/stat.h>

#include "server.h"
#include "cert.h"
#include "prompt.h"


/* NOTES

  Invocation:

  cwca new            - Generate a self-signed rootca cert
  cwca issued         - Show issued certs
  cwca pending        - List pending requests
  cwca revoked        - List revoked certificates
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
	struct server *server;
};

/**************************************************************/

static int mkdir_p(const char *path, int mode);
static struct cwca_opts* cwca_options(int argc, char **argv);

/* Command runners */
static int cwca_help_main(const struct cwca_opts *args);
static int cwca_new_main(const struct cwca_opts *args);
static int cwca_issued_main(const struct cwca_opts *args);
static int cwca_pending_main(const struct cwca_opts *args);
static int cwca_revoked_main(const struct cwca_opts *args);
static int cwca_details_main(const struct cwca_opts *args);
static int cwca_sign_main(const struct cwca_opts *args);
static int cwca_ignore_main(const struct cwca_opts *args);
static int cwca_revoke_main(const struct cwca_opts *args);

static int cwca_new_setup_ca_dirs(const struct cwca_opts *args);

/**************************************************************/

int main(int argc, char **argv)
{
	int err;
	struct cwca_opts *args;

	cert_init();
	log_set(LOG_LEVEL_DEBUG);

	args = cwca_options(argc, argv);
	if (strcmp(args->command, "new") == 0) {
		err = cwca_new_main(args);
	} else if (strcmp(args->command, "issued") == 0) {
		err = cwca_issued_main(args);
	} else if (strcmp(args->command, "pending") == 0) {
		err = cwca_pending_main(args);
	} else if (strcmp(args->command, "revoked") == 0) {
		err = cwca_revoked_main(args);
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

static int mkdir_p(const char *path, int mode)
{
	char *parent, *path2;

	if (mkdir(path, mode) == 0) {
		return 0;
	}

	switch (errno) {
	case EEXIST:
		return 0;
	case ENOENT:
		if (strcmp(path, "/") == 0) {
			return -1;
		}

		path2 = strdup(path);
		parent = dirname(path2);

		if (mkdir_p(parent, mode) == 0) {
			return mkdir(path, mode);
		} else {
			return -1;
		}

	default:
		return -1;
	}
}

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
	cwca->server  = xmalloc(sizeof(struct server));

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
	       "USAGE: cwca [OPTIONS] [COMMAND] [hostname]\n"
	       "\n"
	       "Valid commands are:\n"
	       "\n"
	       "  new     - Setup a new CA, with a self-signed certificate.\n"
	       "  issued  - List signed certificates, by host name and fingerprint.\n"
	       "  pending - List pending signing requests, by host name and fingerprint.\n"
	       "  revoked - Show revoked certificates.\n"
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
	       "OPTIONS can be any or all of the following:\n"
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

	return CWCA_SUCCESS;
}

static int cwca_new_main(const struct cwca_opts *args)
{
	int days   = 365 * 10;

	X509_REQ *request;
	X509 *cert;
	EVP_PKEY *key;

	struct cert_subject subject;
	char fqdn[1024];
	char *confirmation = NULL;

	INFO("Creating new signing key");
	key = cert_generate_key(2048);
	if (!key) {
		CRITICAL("Unable to create new signing key");
		return CWCA_SSL_ERROR;
	}
	if (cert_store_key(key, args->server->key_file) != 0) {
		CRITICAL("Unable to store new signing key in %s", args->server->key_file);
		return CWCA_SSL_ERROR;
	}

	INFO("Creating new Certificate Authority certificate");
	memset(&subject, 0, sizeof(subject));

	subject.type = strdup("Policy Master");
	if (cert_my_hostname(fqdn, 1024) != 0) {
		ERROR("Failed to get local hostname!");
		return CWCA_OTHER_ERR;
	}
	subject.fqdn = fqdn;

	do {
		printf("You will now be asked for server identity information.\n"
		       "\n"
		       "The answers you provide will be used to construct the subject name\n"
		       "of the certificate authority's certificate.\n"
		       "\n");

		cert_prompt_for_subject(&subject);

		printf("\nGenerating new certificate for:\n\n");
		cert_print_subject(stdout, "  ", &subject);
		printf("\nSubject for host certificate will be:\n\n  ");
		cert_print_subject_terse(stdout, &subject);
		printf("\n\n");

		do {
			free(confirmation);
			confirmation = prompt("Is this information correct (yes or no) ? ");
		} while (strcmp(confirmation, "yes") != 0 && strcmp(confirmation, "no") != 0);

	} while (strcmp(confirmation, "yes") != 0);

	request = cert_generate_request(key, &subject);
	if (!request) { return CWCA_SSL_ERROR; }

	cert = cert_sign_request(request, NULL, key, days);
	if (!cert) { return CWCA_SSL_ERROR; }

	if (cert_store_certificate(cert, args->server->cert_file) != 0) {
		CRITICAL("Unable to store CA certificate in %s", args->server->key_file);
		return CWCA_OTHER_ERR;
	}

	unlink(args->server->crl_file);

	if (cwca_new_setup_ca_dirs(args) != CWCA_SUCCESS) {
		return CWCA_OTHER_ERR;
	}

	printf("Complete.\n");
	return CWCA_SUCCESS;
}

static int cwca_issued_main(const struct cwca_opts *args)
{
	X509 **certs;
	size_t i, ncerts = 0;
	char *path;
	struct cert_subject *s;

	path = string("%s/*.pem", args->server->certs_dir);
	certs = cert_retrieve_certificates(path, &ncerts);
	free(path);

	if (ncerts == 0) {
		printf("No certificates have been isued.\n");
		return CWCA_SUCCESS;
	} else if (ncerts < 0) {
		fprintf(stderr, "couldn't list issued certificates.\n");
		return CWCA_OTHER_ERR;
	}

	for (i = 0; i < ncerts; i++) {
		s = cert_certificate_subject(certs[i]);
		printf("%s - %s\t%s\n", s->org, s->fqdn,
		       cert_fingerprint_certificate(certs[i]));
	}
	return CWCA_SUCCESS;
}

static int cwca_pending_main(const struct cwca_opts *args)
{
	X509_REQ **reqs;
	EVP_PKEY *pubkey;
	size_t i, nreqs = 0;
	char *path;
	struct cert_subject *s;

	path = string("%s/*.csr", args->server->requests_dir);
	reqs = cert_retrieve_requests(path, &nreqs);
	free(path);

	if (nreqs == 0) {
		printf("No certificate signing requests pending.\n");
		printf(" (in %s)\n", args->server->requests_dir);
		return CWCA_SUCCESS;
	} else if (nreqs < 0) {
		fprintf(stderr, "couldn't list pending certificate signing requests.\n");
		return CWCA_OTHER_ERR;
	}

	for (i = 0; i < nreqs; i++) {
		pubkey = X509_REQ_get_pubkey(reqs[i]);
		s = cert_request_subject(reqs[i]);
		printf("%s - %s (%s)\n", s->org, s->fqdn,
		       (pubkey ? "verified" : "NOT VERIFIED"));
	}
	return CWCA_SUCCESS;
}

static int cwca_revoked_main(const struct cwca_opts *args)
{
	X509_CRL *crl;
	X509_REVOKED *revoked;
	X509 **certs, *cert;
	size_t ncerts = 0;
	size_t i, j;
	char *files;
	struct cert_subject *s;

	files = string("%s/*.pem", args->server->certs_dir);
	certs = cert_retrieve_certificates(files, &ncerts);
	free(files);

	if (!(crl = cert_retrieve_crl(args->server->crl_file))) {
		ERROR("Unable to retrieve CRL from %s", args->server->crl_file);
		return CWCA_SSL_ERROR;
	}

	for (i = 0; i < sk_X509_CRL_num(crl->crl->revoked); i++) {
		revoked = sk_X509_REVOKED_value(crl->crl->revoked, i);
		if (!revoked) { continue; }

		cert = NULL;
		for (j = 0; j < ncerts; j++) {
			if (ASN1_INTEGER_cmp(X509_get_serialNumber(certs[j]), revoked->serialNumber) == 0) {
				cert = certs[j];
				break;
			}
		}

		if (cert) {
			s = cert_certificate_subject(cert);
			printf("%s   %s - %s\t%s\n",
			       cert_certificate_serial_number(cert),
			       s->org, s->fqdn,
			       cert_fingerprint_certificate(cert));
		} else {
			printf("%s   (no matching cert)\n",
			       cert_certificate_serial_number(cert));
		}
	}

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
	if (!key) {
		fprintf(stderr, "Unable to retrieve CA key from %s\n",
		                args->server->key_file);
		return CWCA_SSL_ERROR;
	}

	request = cert_retrieve_request(req_file);
	cert = cert_retrieve_certificate(cert_file);

	if (cert) {
		printf("Found a certificate\n at %s\n\n", cert_file);
		X509_print_fp(stdout, cert);
	} else if (request) {
		printf("Found a certificate signing request\n at %s\n\n", req_file);
		X509_REQ_print_fp(stdout, request);
	} else {
		printf("Nothing known about %s\n", args->fqdn);
		printf("  (looked in %s\n", req_file);
		printf("         and %s\n", cert_file);
		return CWCA_OTHER_ERR;
	}

	return CWCA_SUCCESS;
}

static int cwca_sign_main(const struct cwca_opts *args)
{
	EVP_PKEY *key;
	X509_REQ *request;
	X509 *cert, *ca_cert;
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

	printf("Retrieving CA certificate.\n");
	ca_cert = cert_retrieve_certificate(args->server->ca_cert_file);

	printf("Loading certificate signing request from %s\n", req_file);
	request = cert_retrieve_request(req_file);
	if (!request) { return CWCA_SSL_ERROR; }

	printf("Signing certificate\n");
	cert = cert_sign_request(request, ca_cert, key, 365*10);
	if (!cert) { return CWCA_SSL_ERROR; }

	printf("Storing signed certifcate in %s\n", cert_file);
	if (cert_store_certificate(cert, cert_file) != 0) {
		perror("Unable to store certificate");
		goto undo;
	}

	printf("Removing certificate signing request.\n");
	if (unlink(req_file) != 0) {
		perror("Unable to remove signing request");
		goto undo;
	}

	return CWCA_SUCCESS;

undo:
	unlink(cert_file);
	return CWCA_OTHER_ERR;
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
	X509_CRL *crl;
	EVP_PKEY *ca_key;
	X509 *ca_cert, *cert;
	char *cert_file;

	if (!args->fqdn) {
		fprintf(stderr, "Invalid invocation.\n"
		                "USAGE: cwca revoke [fqdn]\n");
		return CWCA_OTHER_ERR;
	}

	ca_key = cert_retrieve_key(args->server->key_file);
	if (!ca_key) {
		ERROR("Unable to retrieve CA signing key from %s",
		      args->server->key_file);
		return CWCA_SSL_ERROR;
	}

	ca_cert = cert_retrieve_certificate(args->server->ca_cert_file);
	if (!ca_cert) {
		ERROR("Unable to retrieve CA certificate from %s",
		      args->server->ca_cert_file);
		return CWCA_SSL_ERROR;
	}

	cert_file = string("%s/%s.pem", args->server->certs_dir, args->fqdn);
	if (!cert_file) {
		ERROR("Unable to determine path to certificate for %s", args->fqdn);
		return CWCA_OTHER_ERR;
	}

	cert = cert_retrieve_certificate(cert_file);
	if (!cert) {
		ERROR("Unable to retrieve certificate to revoke (%s)", cert_file);
		return CWCA_SSL_ERROR;
	}

	if (!(crl = cert_retrieve_crl(args->server->crl_file))) {
		if (!(crl = cert_generate_crl(ca_cert))) {
			return CWCA_SSL_ERROR;
		}
	}
	if (cert_revoke_certificate(crl, cert, ca_key) == 0) {
		cert_store_crl(crl, args->server->crl_file);
	}
	X509_CRL_print_fp(stdout, crl);

	/* for the re-issue */
	unlink(cert_file);

	printf("\nRestart policyd for the new revocation list to take effect.\n");
	return CWCA_SUCCESS;
}

static int cwca_new_setup_ca_dirs(const struct cwca_opts *args)
{
	printf("Setting up certificate authority directory structure\n");
	if (mkdir_p(args->server->requests_dir, 0700) != 0) {
		printf("  - Creating requests dir (%s)... failed\n", args->server->requests_dir);
		return CWCA_OTHER_ERR;
	} else {
		printf("  - Creating requests dir (%s)... succeeded\n", args->server->requests_dir);
	}

	if (mkdir_p(args->server->certs_dir, 0700) != 0) {
		printf("  - Creating certs dir (%s)... failed\n", args->server->certs_dir);
		return CWCA_OTHER_ERR;
	} else {
		printf("  - Creating certs dir (%s)... succeeded\n", args->server->requests_dir);
	}

	return CWCA_SUCCESS;
}
