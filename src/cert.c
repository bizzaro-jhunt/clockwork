/*
  Copyright 2011-2013 James Hunt <james@jameshunt.us>

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

#include "cert.h"

#include <glob.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/utsname.h>

#include "prompt.h"

/** NOTES
*/

static int add_ext(STACK_OF(X509_EXTENSION) *sk, int nid, char *value)
{
	X509_EXTENSION *ex;
	ex = X509V3_EXT_conf_nid(NULL, NULL, nid, value);
	if (!ex) {
		return 0;
	}

	sk_X509_EXTENSION_push(sk, ex);
	return 1;
}

static int rand_serial(BIGNUM *b, ASN1_INTEGER *ai)
{
	BIGNUM *btmp;
	int ret = 0;

	btmp = (b ? b : BN_new());
	if (!btmp) { return 0; }

	if (!BN_pseudo_rand(btmp, 64, 0, 0)
	 || (ai && !BN_to_ASN1_INTEGER(btmp, ai))) {
		goto error;
	}

	ret = 1;

error:
	if (!b) {
		BN_free(btmp);
	}

	return ret;
}

static int cert_X509_CRL_get0_by_serial(X509_CRL *crl, X509_REVOKED **revoked_cert, ASN1_INTEGER *serial)
{
	assert(crl);    // LCOV_EXCL_LINE
	assert(serial); // LCOV_EXCL_LINE

	X509_REVOKED *needle = NULL;
	int i, len = sk_X509_REVOKED_num(X509_CRL_get_REVOKED(crl));

	for (i = 0; i < len; i++) {
		needle = sk_X509_REVOKED_value(X509_CRL_get_REVOKED(crl), i);
		if (needle && ASN1_INTEGER_cmp(needle->serialNumber, serial) == 0) {
			if (revoked_cert) {
				*revoked_cert = needle;
			}
			return 0;
		}
	}

	if (revoked_cert) {
		*revoked_cert = NULL;
	}
	return -1;
}

/**
  Initialize the cert module for use
 */
void cert_init(void) {
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
}

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
void cert_deinit(void) {
	ERR_free_strings();
	EVP_cleanup();
}

/**
  Put local node's FQDN in $hostname.

  $hostname is a user-supplied buffer that must be at least
  $len bytes long.  Memory management of $hostname is left
  to the caller.

  On success, fills $hostname with a NULL-terminated string
  and returns 0.  On failure, returns non-zero and the contents
  of $hostname are undefined.
 */
int cert_my_hostname(char *hostname, size_t len)
{
	struct utsname uts;
	struct addrinfo hints, *info;

	if (uname(&uts) != 0) {
		return -1;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;

	if (getaddrinfo(uts.nodename, NULL, &hints, &info) != 0) {
		return -1;
	}

	xstrncpy(hostname, info->ai_canonname, len);
	return 0;
}

/**
  Read a private key from $keyfile

  $keyfile should contain a PEM-encoded OpenSSL private key.
  Clockwork primarily uses RSA keys - and internally, that's
  all it will generate - but DSA keys are supported.

  On success, returns a pointer to the key.
  On failure, returns NULL.
 */
EVP_PKEY* cert_retrieve_key(const char *keyfile)
{
	assert(keyfile); // LCOV_EXCL_LINE

	FILE *fp;
	EVP_PKEY *key = NULL;

	if ((fp = fopen(keyfile, "r")) != NULL) {
		key = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
		fclose(fp);
	} else {
		DEBUG("Unable to open %s for key retrieval", keyfile);
	}

	return key;
}

/**
  Write $key, in PEM format, to $keyfile

  On success, returns 0.  On failure, returns non-zero.
 */
int cert_store_key(EVP_PKEY *key, const char *keyfile)
{
	assert(keyfile); // LCOV_EXCL_LINE

	FILE *fp;

	fp = fopen(keyfile, "w");
	if (!fp) {
		perror("Couldn't save private/public keypair");
		return -1;
	}

	PEM_write_PrivateKey(fp, key, NULL, NULL, 0, NULL, NULL);
	fclose(fp);

	return 0;
}

/**
  Create a new RSA private key, of $bits strength

  The new key will have a bit-strength of $bits, which should
  be a power of 2 (2048, 4096, etc.).  For security reasons,
  $bits should be at least 2048.

  On success, returns a pointer to the new key.
  On failure, returns false.
 */
EVP_PKEY* cert_generate_key(int bits)
{
	EVP_PKEY *key;
	RSA *rsa;

	key = EVP_PKEY_new();
	if (!key) { return NULL; }

	rsa = RSA_generate_key(bits, RSA_F4, NULL, NULL);
	if (!rsa) { return NULL; }

	if (EVP_PKEY_assign_RSA(key, rsa) != 1) {
		return NULL;
	}

	return key;
}

/**
  Read an X.509 certificate request from $csrfile

  $csrfile must be encoded in PEM format.

  On success, returns a pointer to the X.509 certificate request.
  On failure, returns NULL.
 */
X509_REQ* cert_retrieve_request(const char *csrfile)
{
	FILE *fp;
	X509_REQ *request = NULL;

	fp = fopen(csrfile, "r");
	if (fp) {
		request = PEM_read_X509_REQ(fp, NULL, NULL, NULL);
		fclose(fp);
	}
	return request;
}

/**
  Read multiple X.509 certificate requests from files.

  $pathglob should be a shell glob (like "/etc/ssl/\*.csr").
  The glob will be expanded into a list of files, and an X.509
  certificate request will be read from each.  Files must be
  PEM-encoded.  Files that are not, as well as directories
  and special files, are ignored.

  On success, a dynamically-allocated array of pointers to the
  X.509 certificate requests is returned, and the number of pointers
  is stored in the $len result-parameter.

  **Note:** *Success* means *nothing unusual happened.*  If $pathglob
  expands to nothing, or a bunch of files that are not PEM-encoded,
  the function still succeeds, but returns an empty array and sets
  $len to 0.

  On failure, returns NULL and sets $len to -1.
 */
X509_REQ** cert_retrieve_requests(const char *pathglob, size_t *len)
{
	X509_REQ **reqs;
	size_t i;
	glob_t files;

	*len = 0;
	switch (glob(pathglob, GLOB_MARK, NULL, &files)) {
	case GLOB_NOSPACE:
	case GLOB_ABORTED:
		*len = -1;
		/* fall through on purpose */
	case GLOB_NOMATCH:
		globfree(&files);
		return NULL;
	}

	*len = files.gl_pathc;
	reqs = xmalloc(sizeof(X509_REQ*) * (*len + 1));
	for (i = 0; i < *len; i++) {
		reqs[i] = cert_retrieve_request(files.gl_pathv[i]);
	}

	return reqs;
}

/**
  Create a new X.509 certificate request, signed by $key.

  The subject name for the request will be compiled from $subj.

  On success, returns a pointer to the new request.
  On failure, returns NULL.
 */
X509_REQ* cert_generate_request(EVP_PKEY* key, const struct cert_subject *subj)
{
	X509_REQ *request;
	X509_NAME *name = NULL;
	STACK_OF(X509_EXTENSION) *exts = NULL;

	request = X509_REQ_new();
	if (!request) { return NULL; }

	X509_REQ_set_pubkey(request, key);

	if (!X509_REQ_set_version(request, 3)) {
		ERROR("Failed to set X.509 version to 3");
		return NULL;
	}

	name = X509_REQ_get_subject_name(request);
	X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC, (const unsigned char*)subj->country, -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "L",  MBSTRING_ASC, (const unsigned char*)subj->loc,     -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (const unsigned char*)subj->state,   -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC, (const unsigned char*)subj->org,     -1, -1, 0);
	if (strcmp(subj->org_unit, "") != 0) {
		X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC, (const unsigned char*)subj->org_unit,-1, -1, 0);
	}
	X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC, (const unsigned char*)subj->type,    -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)subj->fqdn, -1, -1, 0);

	exts = sk_X509_EXTENSION_new_null();
	//add_ext(exts, NID_key_usage, "critical,digitalSignature,keyEncipherment");
	add_ext(exts, NID_subject_alt_name, subj->fqdn);
	X509_REQ_add_extensions(request, exts);
	sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);

	if (!X509_REQ_sign(request, key, EVP_sha1())) {
		return NULL;
	}

	return request;
}

/**
  Write $request to $csrfile

  The certificate request will be stored in $csrfile in PEM format.

  On success, returns 0.  On failure, returns non-zero.
 */
int cert_store_request(X509_REQ *request, const char *csrfile)
{
	FILE *fp;

	fp = fopen(csrfile, "w");
	if (!fp) { return -1; }

	PEM_write_X509_REQ(fp, request);

	fclose(fp);
	return 0;

}

/**
  Sign $request as the Certificate Authority

  For normal signing operations, $ca_cert and $cakey contain the
  Certificate Authority's X.509 certificate and private key (respectively).
  If $ca_cert is NULL, a self-signed certificate is generated.  `cwca`
  uses this feature when setting up a new Certificate Authority.

  When signing other certificates as the CA, the issuerName of the
  signed certificate is set to the subjectName of the CA certificate.

  In all cases, the signed certificate will be considered valid
  only for a period of $days days.

  On success, returns a pointer to the signed certificate.
  On failure, returns NULL.
 */
X509* cert_sign_request(X509_REQ *request, X509 *ca_cert, EVP_PKEY *cakey, unsigned int days)
{
	EVP_PKEY *pubkey;
	X509 *cert;
	int verified;

	pubkey = X509_REQ_get_pubkey(request);
	if (!pubkey) { return NULL; }

	verified = X509_REQ_verify(request, pubkey);
	if (verified < 0) {
		ERROR("Failed to verify certificate signing request");
		return NULL;
	}
	if (verified == 0) {
		ERROR("Signature mismatch on certificate signing request");
		return NULL;
	}

	cert = X509_new();
	if (!cert) { return NULL; }

	if (!X509_set_version(cert, 3)) {
		ERROR("Failed to set X.509 version to 3");
		return NULL;
	}

	if (!rand_serial(NULL, X509_get_serialNumber(cert))) {
		ERROR("Failed to generate serial number for certificate");
		return NULL;
	}

	if (ca_cert) {
		if (!X509_set_issuer_name(cert, X509_get_subject_name(ca_cert))) {
			ERROR("Failed to set issuer on new certificate");
			return NULL;
		}
	} else {
		if (!X509_set_issuer_name(cert, request->req_info->subject)) {
			ERROR("Failed to set issuer on new self-signed certificate");
			return NULL;
		}
	}

	if (!X509_set_subject_name(cert, request->req_info->subject)) {
		ERROR("Failed to set subject on new certificate");
		return NULL;
	}

	X509_gmtime_adj(X509_get_notBefore(cert), 0);
	X509_gmtime_adj(X509_get_notAfter(cert),(long)60*60*24*days);
	X509_set_pubkey(cert, pubkey);
	EVP_PKEY_free(pubkey);

	if (!X509_sign(cert, cakey, EVP_sha1())) {
		ERROR("Failed to sign certificate as CA");
		return NULL;
	}

	return cert;
}

static struct cert_subject* cert_get_subject(X509_NAME *name)
{
	int idx = -1;
	X509_NAME_ENTRY *entry;
	ASN1_STRING *raw;
	struct cert_subject *subject;

	if (!name) { return NULL; }

	subject = xmalloc(sizeof(struct cert_subject));

	idx = X509_NAME_get_index_by_NID(name, NID_countryName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->country = strdup((const char*)raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_stateOrProvinceName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->state = strdup((const char*)raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_localityName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->loc = strdup((const char*)raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_organizationName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->org = strdup((const char*)raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_organizationalUnitName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->org_unit = strdup((const char*)raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_organizationalUnitName, idx);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->type = strdup((const char*)raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->fqdn = strdup((const char*)raw->data);
	}

	if (!subject->type && subject->org_unit) {
		subject->type = subject->org_unit;
		subject->org_unit = NULL;
	}

	return subject;
}

/**
  Get the subjectName from $request

  On success, a pointer to a new cert_subject object is returned.
  It is the caller's responsibility to free this object (via
  @cert_subject_free).

  On failure, returns NULL.
 */
struct cert_subject *cert_request_subject(X509_REQ *request)
{
	return cert_get_subject(X509_REQ_get_subject_name(request));
}

/**
  Get the subjectName from $cert

  On success, a pointer to a new cert_subject object is returned.
  It is the caller's responsibility to free this object (via 
  @cert_subject_free).

  On failure, returns NULL.
 */
struct cert_subject *cert_certificate_subject(X509 *cert)
{
	return cert_get_subject(X509_get_subject_name(cert));
}

/**
  Get the issuerName from $cert

  On success, a pointer to a new cert_subject object is returned.
  It is the caller's responsibility to free this object (via
  @cert_subject_free).

  On failure, returns NULL.
 */
struct cert_subject *cert_certificate_issuer(X509 *cert)
{
	return cert_get_subject(X509_get_issuer_name(cert));
}

/**
  Get the serial number from $cert

  On success, returns a dynamically-allocated string containing
  the hexadecimal representation of the serial number.  It is the
  caller's responsibility to free this string via `free(3)`.

  On failure, returns NULL.
 */
char* cert_certificate_serial_number(X509 *cert)
{
	BIO *out;
	char *ser;
	int len;

	out = BIO_new(BIO_s_mem());
	if (!out) { return NULL; }

	i2a_ASN1_INTEGER(out, X509_get_serialNumber(cert));
	len = BIO_ctrl_pending(out);
	ser = xmalloc(sizeof(char) * (len + 1));
	BIO_read(out, ser, len);
	ser[len] = '\0';
	BIO_free(out);

	return ser;
}

/**
  Read an X.509 certificate from $certfile.

  $certfile must be encoded in PEM format.

  On success, returns a pointer to the certificate.
  On failure, returns false.
 */
X509* cert_retrieve_certificate(const char *certfile)
{
	X509 *cert = NULL;
	FILE *fp;

	fp = fopen(certfile, "r");
	if (fp) {
		cert = PEM_read_X509(fp, NULL, NULL, NULL);
		fclose(fp);
	}

	return cert;
}

/**
  Read multiple X.509 certificates from files.

  $pathglob should be a shell glob (like "/etc/ssl/\*.pem").
  The glob will be expanded into a list of files, and an X.509
  certificate will be read from each.  Files must be PEM-encoded.
  Files that are not, as well as direcories and special files,
  are ignored.

  On success, a dynamically-allocated array of pointers to the
  X.509 certificates is returned, and the number of pointers is
  stored in the $len result-parameter.

  **Note:** *Success* means *nothing unusual happened.*  If $pathglob
  expands to nothing, or a bunch of files that are not PEM-encoded,
  the function still succeeds, but returns an empty array and sets
  $len to 0.

  On failure, returns NULL and sets $len to -1.
 */
X509** cert_retrieve_certificates(const char *pathglob, size_t *len)
{
	X509 **certs;
	size_t i;
	glob_t files;

	*len = 0;
	switch (glob(pathglob, GLOB_MARK, NULL, &files)) {
	case GLOB_NOSPACE:
	case GLOB_ABORTED:
		*len = -1;
		/* fall through on purpose */
	case GLOB_NOMATCH:
		globfree(&files);
		return NULL;
	}

	*len = files.gl_pathc;
	certs = xmalloc(sizeof(X509*) * (*len + 1));
	for (i = 0; i < *len; i++) {
		certs[i] = cert_retrieve_certificate(files.gl_pathv[i]);
	}

	return certs;
}

/**
  Write $cert to $certfile.

  The certificate will be stored in $certfile in PEM format.

  On success, returns 0.  On failure, returns non-zero.
 */
int cert_store_certificate(X509 *cert, const char *certfile)
{
	FILE *fp;
	fp = fopen(certfile, "w");
	if (!fp) { return -1; }

	if (PEM_write_X509(fp, cert) != 1) {
		return 1;
	}

	fclose(fp);
	return 0;
}

/**
  Get the fingerprint of $cert.

  On success, returns the fingerprint, in hexadecimal format.
  It is the caller's responsibility to free this string.

  On failure, returns NULL.
 */
char* cert_fingerprint_certificate(X509 *cert)
{
	char *data, *f;
	unsigned char buf[EVP_MAX_MD_SIZE];
	unsigned int len;
	unsigned int i;
	const EVP_MD *digest = EVP_sha1();

	if (!X509_digest(cert, digest, buf, &len)) {
		/* out of memory... */
		return NULL;
	}

	data = xmalloc((len*3+1) * sizeof(char));
	for (i = 0, f = data; i < len; i++, f += 3) {
		snprintf(f, 4, "%02x%c", buf[i], ((i+1) == len ? '\0' : ':'));
	}

	return data;
}

/**
  Prompt user for a certificate subject.

  This function is used during the generation of a certificate signing
  request.  It populates the cert_subject argument with the values
  supplied by the end user.

  On success, returns 0.  On failure, returns non-zero and the
  contents of $subject is undefined.
 */
int cert_prompt_for_subject(struct cert_subject *subject, FILE *io)
{
	if (!io) io = stdin;

	free(subject->country);
	subject->country = prompt("Country (C): ", io);

	free(subject->state);
	subject->state = prompt("State / Province (ST): ", io);

	free(subject->loc);
	subject->loc = prompt("Locality / City (L): ", io);

	free(subject->org);
	subject->org = prompt("Organization (O): ", io);

	free(subject->org_unit);
	subject->org_unit = prompt("Org. Unit (OU): ", io);

	return 0;
}

int cert_read_subject(struct cert_subject *subject, FILE *io)
{
	if (!io) io = stdin;

	free(subject->country);
	subject->country = prompt(NULL, io);

	free(subject->state);
	subject->state = prompt(NULL, io);

	free(subject->loc);
	subject->loc = prompt(NULL, io);

	free(subject->org);
	subject->org = prompt(NULL, io);

	free(subject->org_unit);
	subject->org_unit = prompt(NULL, io);

	return 0;
}

/**
  Print $subject to $io, using the terse format.

  The terse format for subject names is a comma-separated set of
  K=value pairs, like "C=US, ST=NY, L=New York, O=Nifty, CN=c.example.net"

  Always returns 0.
 */
int cert_print_subject_terse(FILE *io, const struct cert_subject *subject)
{
	if (strcmp(subject->org_unit, "") == 0) {
		fprintf(io, "C=%s, ST=%s, L=%s, O=%s, OU=%s, CN=%s",
		        subject->country, subject->state, subject->loc,
		        subject->org, subject->type, subject->fqdn);
	} else {
		fprintf(io, "C=%s, ST=%s, L=%s, O=%s, OU=%s, OU=%s, CN=%s",
		        subject->country, subject->state, subject->loc,
		        subject->org, subject->type, subject->org_unit, subject->fqdn);
	}
	return 0;
}

/**
  Print $subject to $io, using the expanded format.

  The expanded format for subject names places each component on its own
  line, and is suitable for display to end users.  For example:

  <code>
  Country:          US
  State / Province: NY
  Locality / City:  New York
  Organization:     Nifty
  Org. Unit:
  Host Name:        c.example.net
  </code>

  Callers can influence the output by passing $prefix, which will be
  output before every line.

  Always returns 0.
 */
int cert_print_subject(FILE *io, const char *prefix, const struct cert_subject *subject)
{
	fprintf(io, "%sCountry:          %s\n"
	            "%sState / Province: %s\n"
	            "%sLocality / City:  %s\n"
	            "%sOrganization:     %s\n"
	            "%sOrg. Unit:        %s\n"
	            "%sHost Name:        %s\n",
	            prefix, subject->country,
	            prefix, subject->state,
	            prefix, subject->loc,
	            prefix, subject->org,
	            prefix, subject->org_unit,
	            prefix, subject->fqdn);
	return 0;
}

/**
  Read an X.509 certificate revocation list from $crlfile.

  $crlfile must be encoded in PEM format.

  On success, returns a pointer to the revocation list.
  On failure, returns NULL.
 */
X509_CRL* cert_retrieve_crl(const char *crlfile)
{
	FILE *fp;
	X509_CRL *crl = NULL;

	fp = fopen(crlfile, "r");
	if (fp) {
		crl = PEM_read_X509_CRL(fp, NULL, NULL, NULL);
		fclose(fp);
	}
	return crl;
}

/**
  Create a new (empty) X.509 revocation list.

  The new CRL will be signed by the $ca_cert certificate, but
  contain no revoked certificates.  See @cert_revoke_certificate.

  On success, returns a pointer to the CRL.
  On failure, returns NULL.
 */
X509_CRL* cert_generate_crl(X509 *ca_cert)
{
	X509_CRL *crl;

	if (!(crl = X509_CRL_new())) {
		return NULL;
	}

	if (!X509_CRL_set_issuer_name(crl, X509_get_subject_name(ca_cert))) {
		X509_CRL_free(crl);
		return NULL;
	}

	return crl;
}

/**
  Write $crl to $crlfile.

  The certificate revocation list will be stored in $crlfile
  in PEM format.

  On success, returns 0.  On failure returns -1.
 */
int cert_store_crl(X509_CRL *crl, const char *crlfile)
{
	FILE *fp;

	fp = fopen(crlfile, "w");
	if (!fp) { return -1; }

	PEM_write_X509_CRL(fp, crl);

	fclose(fp);
	return 0;
}

/**
  Revoke $cert, and add it to $crl.

  In order to decrypt and subsequently re-encrypt the CRL,
  the $key parameter must be the key used to sign the list
  initially.

  On success, returns 0.  On failure, returns non-zero.
 */
int cert_revoke_certificate(X509_CRL *crl, X509 *cert, EVP_PKEY *key)
{
	assert(crl); // LCOV_EXCL_LINE
	assert(cert); // LCOV_EXCL_LINE

	X509_REVOKED *revoked_cert;
	ASN1_ENUMERATED *reason_code;
	ASN1_TIME *revoked_at; /* now */

	if (cert_X509_CRL_get0_by_serial(crl, &revoked_cert, X509_get_serialNumber(cert)) == 0) {
		DEBUG("Already revoked...");
		/* already revoked */
		return 1;
	}

	revoked_cert = X509_REVOKED_new();
	if (!revoked_cert) { goto error; }

	revoked_at = ASN1_UTCTIME_new();
	if (!(revoked_at = ASN1_UTCTIME_new())
	 || !ASN1_UTCTIME_set(revoked_at, time(NULL))
	 || !X509_REVOKED_set_revocationDate(revoked_cert, revoked_at)) {
		goto error;
	}

	if (!(reason_code = ASN1_ENUMERATED_new())
	 || !ASN1_ENUMERATED_set(reason_code, OCSP_REVOKED_STATUS_SUPERSEDED)
	 || !X509_REVOKED_add1_ext_i2d(revoked_cert, NID_crl_reason, reason_code, 0, 0)) {
		goto error;
	}

	if (!X509_REVOKED_set_serialNumber(revoked_cert, X509_get_serialNumber(cert))) {
		goto error;
	}

	X509_CRL_add0_revoked(crl, revoked_cert);
	X509_CRL_sort(crl);
	X509_CRL_set_lastUpdate(crl, revoked_at);
	if (!X509_time_adj(revoked_at, 10 * 365 * 86400, NULL)) {
		goto error;
	}
	X509_CRL_set_nextUpdate(crl, revoked_at);

	if (!X509_CRL_sign(crl, key, EVP_sha1())) {
		goto error;
	}

	return 0;

error:
	if (revoked_cert) {
		X509_REVOKED_free(revoked_cert);
	}
	return -1;
}

/**
  Check if $cert is revoked according to $crl.

  Returns non-zero if $cert is revoked, or 0 if not.
 */
int cert_is_revoked(X509_CRL *crl, X509 *cert)
{
	return cert_X509_CRL_get0_by_serial(crl, NULL, X509_get_serialNumber(cert));
}

/**
  Free cert_subject structure $s.
 */
void cert_subject_free(struct cert_subject *s)
{
	if (s) {
		free(s->country);
		free(s->state);
		free(s->loc);
		free(s->org);
		free(s->org_unit);
		free(s->type);
		free(s->fqdn);
	}
	free(s);
}

