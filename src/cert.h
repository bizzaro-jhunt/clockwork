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

/**
  Certificate subjectName
 */
struct cert_subject {
	char *country;  /* Country;              C=<country> */
	char *state;    /* State or Province;   ST=<state> */
	char *loc;      /* Locality or City;     L=<loc> */
	char *org;      /* Organization;         O=<org> */
	char *org_unit; /* Organizational Unit; OU=<org_unit> */
	char *type;     /* Type of certificate; OU=<type> */
	char *fqdn;     /* Full domain name;    CN=<fqdn> */
};

void cert_init(void);
void cert_deinit(void);

int cert_my_hostname(char *hostname, size_t len);

EVP_PKEY* cert_retrieve_key(const char *keyfile);
int cert_store_key(EVP_PKEY *key, const char *keyfile);
EVP_PKEY* cert_generate_key(int bits);

X509_REQ* cert_retrieve_request(const char *csrfile);
X509_REQ** cert_retrieve_requests(const char *pathglob, size_t *len);
X509_REQ* cert_generate_request(EVP_PKEY *key, const struct cert_subject *subj);
int cert_store_request(X509_REQ *request, const char *csrfile);
X509* cert_sign_request(X509_REQ *request, X509 *ca_cert, EVP_PKEY *key, unsigned int days);

struct cert_subject* cert_request_subject(X509_REQ *request);
struct cert_subject* cert_certificate_subject(X509 *cert);
struct cert_subject* cert_certificate_issuer(X509 *cert);
char* cert_certificate_serial_number(X509 *cert);

X509* cert_retrieve_certificate(const char *certfile);
X509** cert_retrieve_certificates(const char *pathglob, size_t *len);
int cert_store_certificate(X509 *cert, const char *certfile);

char* cert_fingerprint_certificate(X509 *cert);
int cert_prompt_for_subject(struct cert_subject *s, FILE *io);
int cert_read_subject(struct cert_subject *s, FILE *io);
int cert_print_subject_terse(FILE *io, const struct cert_subject *s);

int cert_print_subject(FILE *io, const char *prefix, const struct cert_subject *s);

X509_CRL* cert_retrieve_crl(const char *crlfile);
X509_CRL* cert_generate_crl(X509 *ca_cert);
int cert_store_crl(X509_CRL *crl, const char *crlfile);
int cert_revoke_certificate(X509_CRL *crl, X509 *cert, EVP_PKEY *key);
int cert_is_revoked(X509_CRL *crl, X509 *cert);

void cert_subject_free(struct cert_subject *s);

#endif
