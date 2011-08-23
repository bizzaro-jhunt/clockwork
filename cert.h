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

/** @file cert.h

  The cert module provides SSL certificate services to the rest
  of the system, effectively wrapping the details of the OpenSSL
  implementation.  Specifically, the cert module provides facilities
  for generating, storing and retrieving private keys, certificates,
  signing requests and revocation lists.

 */

/**
  Certificate subjectName
 */
struct cert_subject {
	/** Country (C=) component */
	char *country;
	/** State/Province (ST=) component */
	char *state;
	/** Locality/City (L=) component */
	char *loc;
	/** Organization (O=) component */
	char *org;
	/** Organizational Unit (OU=) component */
	char *org_unit;
	/** Type of certificate (used internally; OU=) */
	char *type;
	/** Fully-qualified domain name of the subject (CN=) */
	char *fqdn;
};

/**
  Initialize the cert module for use
 */
void cert_init(void);

/**
  Deinitialize the cert module

  This routine must be called to properly clean up
  memory used by OpenSSL.  In practice there is almost
  no use for it (yet) since the policyd daemon always
  needs OpenSSL routines to be around, and the client
  side tools are not long-running.

  It's really defined so that the unit tests can call
  it and not report false-positive memory leaks.
 */
void cert_deinit(void);

/**
  Retrieve the local nodes hostname (FQDN)

  cert_my_hostname fills a user-supplied string buffer with the
  local node's fully-qualified domain name (i.e. www.example.net),
  for use in the cert_subject for a certificate signing request.

  @param  hostname    A buffer (at least \a len bytes long)
  @param  len         Length of the \a hostname buffer

  @returns 0 on success, non-zero on failure.  On failure,
           the contents of \a hostname are undefined.
 */
int cert_my_hostname(char *hostname, size_t len);

/**
  Retrieve a private key from a PEM file

  @param  keyfile   The path to the PEM file containing the key.

  @returns an opaque handle for an OpenSSL private key structure,
           or NULL on failure.
 */
EVP_PKEY* cert_retrieve_key(const char *keyfile);

/**
  Store a private key in a file, in PEM format

  @param  key       The OpenSSL private key to store.
  @param  keyfile   Path to the file to store \a key in.

  @returns 0 on success, or non-zero on failure.
 */
int cert_store_key(EVP_PKEY *key, const char *keyfile);

/**
  Generate a new private key

  @param  bits   Number of bits to use for key strength.
                 Usually, this is a power of 2, and 2048 is the
                 current (as of 2011) recommendation.

  @returns an opaque handle for an OpenSSL private key structure,
           or NULL on failure.
 */
EVP_PKEY* cert_generate_key(int bits);

/**
  Retrieve a Certificate Signing Request from a PEM file.

  @param  csrfile   The pat to the PEM file containing the CSR.

  @returns a pointer to a CSR structure populated with the details
           of the CSR contained in \a csrfile, or NULL on failure.
 */
X509_REQ* cert_retrieve_request(const char *csrfile);

/**
  Retrieve multiple Certificate Signing Requests.

  This function accepts a shell path glob (like "/etc/ssl/\*.csr"),
  expands it, and attempts to read in a CSR from each resulting file.
  Any files found that are not PEM files (as well as directories,
  block devices, etc.) are ignored.

  The number of valid signing requests read is returned through
  the \a len result parameter.  If the function call fails, \a len
  will be set to -1.

  It is the responsibility of the caller to free the returnd array.

  @param  pathglob   Shell path glob to match against.
  @param  len        Pointer to a size_t in which to store the number
                     of valid signing requests read in.

  @returns an array of CSR structures, or NULL on failure.
 */
X509_REQ** cert_retrieve_requests(const char *pathglob, size_t *len);

/**
  Generate a Certificate Signing Request

  @param  key   Private key used to sign the CSR.
  @param  subj  A cert_subject representing the full subjectName
                to use for the CSR (and the signed request).

  @returns a CSR structure, or NULL on failure.
 */
X509_REQ* cert_generate_request(EVP_PKEY *key, const struct cert_subject *subj);

/**
  Store a Certificate Signing Request, in PEM format.

  @param  request   The signing request to store.
  @param  csrfile   The file in which to store the request, in PEM format.

  @returns 0 on success, non-zero on failure.
 */
int cert_store_request(X509_REQ *request, const char *csrfile);

/**
  Sign a certificate request as the Certificate Authority.

  If the \a ca_cert parameter is NULL, generate a self-signed certificate.
  The cwca utility uses this feature when setting up a new CA certificate.
  If \a ca_cert is not null, its subjectName will become the issuerName for
  the newly signed certificate.

  @param  request    The X509 Certificate Signing Request to sign
  @param  ca_cert    CA Certificate to sign with.
  @param  key        Private CA key to sign with.
  @param  days       Number of days the signed certificate will be valid.

  @returns a signed certificate on success, or NULL on failure.
 */
X509* cert_sign_request(X509_REQ *request, X509 *ca_cert, EVP_PKEY *key, unsigned int days);

/**
  Retrieve the subjectName from a signing request.

  The returned string is dynamically allocated; it is the caller's
  responsibility to free it.

  @param  request    Certificate signing request.

  @returns a dynamically allocated string containing the organization
           name (O=) and the hostname (CN=), separated by " - ", or
           NULL on failure.
 */
char* cert_request_subject_name(X509_REQ *request);

/**
  Retrieve the subjectName from a certificate.

  The returned string is dynamically allocated; it is the caller's
  responsibility to free it.

  @param  cert       Certificate

  @returns a dynamically allocated string containing the organization
           name (O=) and the hostname (CN=), separated by " - ", or
           NULL on failure.
 */
char* cert_certificate_subject_name(X509 *cert);

/**
  Retrieve the issuerName from a certificate.

  The returned string is dynamically allocated; it is the caller's
  responsibility to free it.

  @param  cert       Certificate

  @returns a dynamically allocated string containing the organization
           name (O=) and the hostname (CN=), separated by " - ", or
           NULL on failure.
 */
char* cert_certificate_issuer_name(X509 *cert);

/**
  Retrieve the serial number from a certficate.

  The returned string is dynamically allocated; it is the caller's
  responsibility to free it.

  @param  cert       Certificate

  @returns a dynamically allocated string containing the hex
           representation of the certificate serial number,
           or NULL on failure.
 */
char* cert_certificate_serial_number(X509 *cert);

/**
  Retrieve a Certificate from a PEM file.

  @param  certfile   File to read the certificate from.

  @returns a certificate structure on success, or NULL on failure.
 */
X509* cert_retrieve_certificate(const char *certfile);

/**
  Retrieve multiple Certficates.

  This function accepts a shell path glob (like "/etc/ssl/\*.pem"),
  expands it, and attempts to read in a certificate from each
  resulting file.  Any files found that are not PEM files are ignored.

  The number of valid certificates read is returned through the
  \a len result parameter.  If the function call fails, \a len
  will be set to -1.

  It is the responsibility of the caller to free the returned array.

  @param  pathglob   Shell path glob to match against.
  @param  len        Pointer to a size_t in which to store the number
                     of valid certificates read in.

  @returns an array of certificate structures, or NULL on failure.
 */
X509** cert_retrieve_certificates(const char *pathglob, size_t *len);

/**
  Store a Certificate in a PEM file.

  @param  cert      The certificate to store.
  @param  certfile  The file in which to store the certificate.

  @returns 0 on succes, non-zero on failure.
 */
int cert_store_certificate(X509 *cert, const char *certfile);

/**
  Retrieve the fingerprint of a signed certificate.

  The returned string is dynamically allocated; it is the responsibility
  of the caller to free it.

  @param  cert   The certificate to fingerprint.

  @returns a string containing the hex representation of the unique
           certificate fingerprint (SHA1) on success, or NULL on failure.
 */
char* cert_fingerprint_certificate(X509 *cert);

/**
  Prompt user for a certificate subject.

  This function is used during the generation of a certificate signing
  request.  It populates the cert_subject argument with the values
  supplied by the end user.

  @param  s     A cert_subject structure to store the components of the
                subject (as entered by the user).

  @returns 0 on success, non-zero on failure.
 */
int cert_prompt_for_subject(struct cert_subject *s);

/**
  Print a certificate subject name (terse format).

  The terse format for subject names is a comma-separated set of
  K=value pairs, like "C=US, ST=NY, L=New York, O=Nifty, CN=c.example.net"

  @param  io      FILE pointer to print the subject name to.
  @param  s       Certificate subject to print.
 */
int cert_print_subject_terse(FILE *io, const struct cert_subject *s);

/**
  Print a certificate subject name (expanded format).

  The expanded format for subject names places each component on its own
  line, and is suitable for display to end users.  For example:

  @verbatim
  Country:          US
  State / Province: NY
  Locality / City:  New York
  Organization:     Nifty
  Org. Unit:
  Host Name:        c.example.net
  @endverbatim

  @param  io      FILE pointer to print the subject name to.
  @param  prefix  A string prefix to use on every line;  useful for
                  indenting the output (i.e. via "   ")
  @param  s       Certificate subject to print.
 */
int cert_print_subject(FILE *io, const char *prefix, const struct cert_subject *s);

/**
  Retrieve a Certificate Revocation List from a PEM file.

  @param  crlfile    Path to the file containing the CRL.

  @returns a CRL structure on success, or NULL on failure.
 */
X509_CRL* cert_retrieve_crl(const char *crlfile);

/**
  Generate a Certificate Revocation List.

  The new CRL will be signed by the \a ca_cert certificate, but will
  contain no revocations.  @see cert_revoke_certificate.

  @param  ca_cert    CA certificate to sign the CRL with.

  @returns a CRL structure on success, or NULL on failure.
 */
X509_CRL* cert_generate_crl(X509 *ca_cert);

/**
  Store a Certificate Revocation List in a PEM file.

  @param  crl       Certificate revocation list to store.
  @param  crlfile   Path to the file to store the CRL in.

  @returns 0 on success, non-zero on failure.
 */
int cert_store_crl(X509_CRL *crl, const char *crlfile);

/**
  Revoke a Certificate via a CRL.

  This function adds a certificate to a Certificate Revocation List,
  so that it cannot be used again by any client.  The \a key parameter
  is required to decrypt / encrypt the CRL.

  @param  crl    Certificate Revocation List.
  @param  cert   Certificate to revoke.
  @param  key    Key to re-signe the CRL with after revocation.

  @returns 0 on success, non-zero on failure.
 */
int cert_revoke_certificate(X509_CRL *crl, X509 *cert, EVP_PKEY *key);

int cert_is_revoked(X509_CRL *crl, X509 *cert);

#endif
