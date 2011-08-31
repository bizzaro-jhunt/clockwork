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
	int i, len = sk_X509_CRL_num(crl->crl->revoked);

	for (i = 0; i < len; i++) {
		needle = sk_X509_REVOKED_value(crl->crl->revoked, i);
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


void cert_init(void) {
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
}

void cert_deinit(void) {
	ERR_free_strings();
	EVP_cleanup();
}

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

int cert_store_request(X509_REQ *request, const char *csrfile)
{
	FILE *fp;

	fp = fopen(csrfile, "w");
	if (!fp) { return -1; }

	PEM_write_X509_REQ(fp, request);

	fclose(fp);
	return 0;

}

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
		subject->country = strdup(raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_stateOrProvinceName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->state = strdup(raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_localityName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->loc = strdup(raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_organizationName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->org = strdup(raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_organizationalUnitName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->org_unit = strdup(raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_organizationalUnitName, idx);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->type = strdup(raw->data);
	}

	idx = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
	entry = X509_NAME_get_entry(name, idx);
	if ((raw = X509_NAME_ENTRY_get_data(entry))) {
		subject->fqdn = strdup(raw->data);
	}

	if (!subject->type && subject->org_unit) {
		subject->type = subject->org_unit;
		subject->org_unit = NULL;
	}

	return subject;
}

struct cert_subject *cert_request_subject(X509_REQ *request)
{
	return cert_get_subject(X509_REQ_get_subject_name(request));
}

struct cert_subject *cert_certificate_subject(X509 *cert)
{
	return cert_get_subject(X509_get_subject_name(cert));
}

struct cert_subject *cert_certificate_issuer(X509 *cert)
{
	return cert_get_subject(X509_get_issuer_name(cert));
}

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

int cert_prompt_for_subject(struct cert_subject *subject)
{
	free(subject->country);
	subject->country = prompt("Country (C): ");

	free(subject->state);
	subject->state = prompt("State / Province (ST): ");

	free(subject->loc);
	subject->loc = prompt("Locality / City (L): ");

	free(subject->org);
	subject->org = prompt("Organization (O): ");

	free(subject->org_unit);
	subject->org_unit = prompt("Org. Unit (OU): ");

	return 0;
}

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

int cert_store_crl(X509_CRL *crl, const char *crlfile)
{
	FILE *fp;

	fp = fopen(crlfile, "w");
	if (!fp) { return -1; }

	PEM_write_X509_CRL(fp, crl);

	fclose(fp);
	return 0;
}

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

int cert_is_revoked(X509_CRL *crl, X509 *cert)
{
	return cert_X509_CRL_get0_by_serial(crl, NULL, X509_get_serialNumber(cert));
}

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

