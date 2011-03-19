#ifndef CERT_H
#define CERT_H

#include "clockwork.h"

#include <assert.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/ocsp.h>

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
X509_REQ** cert_retrieve_requests(const char *pathglob, size_t *len);
X509_REQ* cert_generate_request(EVP_PKEY *key, const struct cert_subject *subj);
int cert_store_request(X509_REQ *request, const char *csrfile);

/**
  Sign a certificate request as the Certificate Authority.

  If the \a ca_cert parameter is NULL, generate a self-signed certificate.
  The cwca utility uses this feature when setting up a new CA certificate.
  If \a ca_cert is not null, its subjectName will become the issuerName for
  the newly signed certificate.

  \param  request    The X509 Certificate Signing Request to sign
  \param  ca_cert    CA Certificate to sign with.
  \param  key        Private CA key to sign with.
  \param  days       Number of days the signed certificate will be valid.
 */
X509* cert_sign_request(X509_REQ *request, X509 *ca_cert, EVP_PKEY *key, unsigned int days);

char* cert_request_subject_name(X509_REQ *request);
char* cert_certificate_subject_name(X509 *cert);
char* cert_certificate_issuer_name(X509 *cert);
char* cert_certificate_serial_number(X509 *cert);

X509* cert_retrieve_certificate(const char *certfile);
X509** cert_retrieve_certificates(const char *pathglob, size_t *len);
int cert_store_certificate(X509 *cert, const char *certfile);

char* cert_fingerprint_certificate(X509 *cert);

int cert_prompt_for_subject(struct cert_subject *s);
int cert_print_subject_terse(FILE *io, const struct cert_subject *s);
int cert_print_subject(FILE *io, const char *prefix, const struct cert_subject *s);

X509_CRL* cert_retrieve_crl(const char *crlfile);
X509_CRL* cert_generate_crl(X509 *ca_cert);
int cert_store_crl(X509_CRL *crl, const char *crlfile);

int cert_revoke_certificate(X509_CRL *crl, X509 *cert, EVP_PKEY *key);
X509_CRL* cert_new_crl(X509 *ca_cert);

#endif
