#ifndef CERT_H
#define CERT_H

#include "clockwork.h"

#include <assert.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

void cert_init(void);
int cert_my_hostname(char *hostname, size_t len);

EVP_PKEY* cert_retrieve_key(const char *keyfile);
EVP_PKEY* cert_generate_key(int bits);

X509_REQ* cert_retrieve_request(const char *csrfile);
X509_REQ* cert_generate_request(EVP_PKEY *key, const char *o);
int cert_store_request(X509_REQ *request, const char *csrfile);
X509* cert_sign_request(X509_REQ *request, EVP_PKEY *key, unsigned int days);
char* cert_request_subject_name(X509_REQ *request);
char* cert_certificate_subject_name(X509 *cert);
char* cert_certificate_issuer_name(X509 *cert);

X509* cert_retrieve_certificate(const char *certfile);
int cert_store_certificate(X509 *cert, const char *certfile);

char* cert_fingerprint_certificate(X509 *cert);

#endif
