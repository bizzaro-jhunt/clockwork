#include "cert.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/utsname.h>

/** NOTES
*/

static int cert_my_hostname(char *hostname, size_t len)
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

void cert_init(void) {
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
}

EVP_PKEY* cert_retrieve_key(const char *keyfile)
{
	assert(keyfile);

	FILE *fp;
	EVP_PKEY *key = NULL;

	if ((fp = fopen(keyfile, "r")) != NULL) {
		key = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
		fclose(fp);
		return key;
	}
	INFO("Generating public / private keypair");

	key = cert_generate_key(2048);
	if (!key) { return NULL; }

	fp = fopen(keyfile, "w");
	if (!fp) {
		perror("Couldn't save private/public keypair");
		return NULL;
	}

	PEM_write_PrivateKey(fp, key, NULL, NULL, 0, NULL, NULL);
	fclose(fp);
	return key;
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

X509_REQ* cert_generate_request(EVP_PKEY* key, const char *o)
{
	X509_REQ *request;
	X509_NAME *name = NULL;
	STACK_OF(X509_EXTENSION) *exts = NULL;
	char fqdn[1024];

	if (cert_my_hostname(fqdn, 1024) != 0) {
		return NULL;
	}

	request = X509_REQ_new();
	if (!request) { return NULL; }

	X509_REQ_set_pubkey(request, key);

	name = X509_REQ_get_subject_name(request);
	X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC, (const unsigned char*)o, -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)fqdn, -1, -1, 0);

	exts = sk_X509_EXTENSION_new_null();
	//add_ext(exts, NID_key_usage, "critical,digitalSignature,keyEncipherment");
	add_ext(exts, NID_subject_alt_name, fqdn);
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

X509* cert_sign_request(X509_REQ *request, EVP_PKEY *cakey, unsigned int days)
{
	EVP_PKEY *pubkey;
	X509 *cert;
	X509_NAME *issuer = NULL;
	int verified;
	char fqdn[1024];

	if (cert_my_hostname(fqdn, 1024) != 0) {
		return NULL;
	}

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

	/*
	if (!X509_set_version(cert, 2)) {
		ERROR("Failed to set X.509 version to 2");
		return NULL;
	}
	*/

	if (!rand_serial(NULL, X509_get_serialNumber(cert))) {
		ERROR("Failed to generate serial number for certificate");
		return NULL;
	}

	issuer = X509_get_issuer_name(cert);
	X509_NAME_add_entry_by_txt(issuer, "O",  MBSTRING_ASC, (const unsigned char*)"Clockwork Policy Master", -1, -1, 0);
	X509_NAME_add_entry_by_txt(issuer, "CN", MBSTRING_ASC, (const unsigned char*)fqdn, -1, -1, 0);

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

static char* cert_get_name(X509_NAME *name)
{
	int idx;
	X509_NAME_ENTRY *o, *cn;
	ASN1_STRING *raw_o, *raw_cn;

	if (!name) { return NULL; }

	idx = X509_NAME_get_index_by_NID(name, NID_organizationName, -1);
	o = X509_NAME_get_entry(name, idx);
	raw_o = X509_NAME_ENTRY_get_data(o);

	idx = X509_NAME_get_index_by_NID(name, NID_commonName, idx);
	cn = X509_NAME_get_entry(name, idx);
	raw_cn = X509_NAME_ENTRY_get_data(cn);

	return string("%s - %s", raw_o->data, raw_cn->data);
}

char *cert_request_subject_name(X509_REQ *request)
{
	return cert_get_name(X509_REQ_get_subject_name(request));
}

char* cert_certificate_subject_name(X509 *cert)
{
	return cert_get_name(X509_get_subject_name(cert));
}

char* cert_certificate_issuer_name(X509 *cert)
{
	return cert_get_name(X509_get_issuer_name(cert));
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

	data = xmalloc((len*3) * sizeof(char));
	for (i = 0, f = data; i < len; i++, f += 3) {
		snprintf(f, 4, "%02x%c", buf[i], ((i+1) == len ? '\0' : ':'));
	}

	return data;
}
