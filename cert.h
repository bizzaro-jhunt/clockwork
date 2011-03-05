#ifndef CERT_H
#define CERT_H

#include "clockwork.h"

#include <assert.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

struct cert_subject {
	char *country;
	char *state;
	char *loc;
	char *org;
	char *org_unit;
	char *type;
	char *fqdn;
};

void cert_init(void);
int cert_my_hostname(char *hostname, size_t len);

EVP_PKEY* cert_retrieve_key(const char *keyfile);
int cert_store_key(EVP_PKEY *key, const char *keyfile);
EVP_PKEY* cert_generate_key(int bits);

X509_REQ* cert_retrieve_request(const char *csrfile);
X509_REQ* cert_generate_request(EVP_PKEY *key, const struct cert_subject *subj);
int cert_store_request(X509_REQ *request, const char *csrfile);
X509* cert_sign_request(X509_REQ *request, EVP_PKEY *key, unsigned int days);
char* cert_request_subject_name(X509_REQ *request);
char* cert_certificate_subject_name(X509 *cert);
char* cert_certificate_issuer_name(X509 *cert);

X509* cert_retrieve_certificate(const char *certfile);
int cert_store_certificate(X509 *cert, const char *certfile);

char* cert_fingerprint_certificate(X509 *cert);

int cert_prompt_for_subject(struct cert_subject *s);
int cert_print_subject_terse(FILE *io, const struct cert_subject *s);
int cert_print_subject(FILE *io, const char *prefix, const struct cert_subject *s);

#endif
