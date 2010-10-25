#ifndef _NET_H
#define _NET_H

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

int verify_callback(int ok, X509_STORE_CTX *store);
long post_connection_check(SSL *ssl, char *host);

#endif
