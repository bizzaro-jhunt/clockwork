#include "test.h"
#include "assertions.h"
#include "../../cert.h"

#include "cert.h"

#define X509_ROOT  DATAROOT "/x509"
#define CA_KEY     X509_ROOT "/ca/key.pem"
#define CA_CERT    X509_ROOT "/ca/cert.pem"
#define CA_CRL     X509_ROOT "/ca/crl.pem"

#define CA_ID "Clockwork Root CA - cfm.niftylogic.net"

static void assert_cert_id(X509 *cert, const char *issuer, const char *subject)
{
	char *issuer_actual, *subject_actual;

	assert_not_null("cert to validate ID for is not NULL", cert);
	assert_not_null("issuer specified in call to assert_cert_id", issuer);
	assert_not_null("subject specified in call to assert_cert_id", subject);

	issuer_actual = cert_certificate_issuer_name(cert);
	assert_not_null("cert issuer is present", issuer_actual);
	assert_str_eq("verify certificate issuer", issuer_actual, issuer);

	subject_actual = cert_certificate_subject_name(cert);
	assert_not_null("cert subject is present", subject_actual);
	assert_str_eq("verify certificate subject", subject_actual, subject);

	free(issuer_actual);
	free(subject_actual);
}

void test_cert_key_retrieve()
{
	EVP_PKEY *key;

	test("Cert: Private Key retrieval");

	key = cert_retrieve_key(X509_ROOT "/keys/rsa128.pem");
	assert_not_null("128-bit RSA key retrieval", key);
	assert_int_eq("rsa128.pem should be an RSA key", key->type, EVP_PKEY_RSA);
	assert_int_eq("rsa128.pem should be 128-bit", RSA_size(key->pkey.rsa) * 8, 128);
	EVP_PKEY_free(key);

	key = cert_retrieve_key(X509_ROOT "/keys/rsa1024.pem");
	assert_not_null("1024-bit RSA key retrieval", key);
	assert_int_eq("rsa1024.pem should be an RSA key", key->type, EVP_PKEY_RSA);
	assert_int_eq("rsa1024.pem should be 1024-bit", RSA_size(key->pkey.rsa) * 8, 1024);
	EVP_PKEY_free(key);

	key = cert_retrieve_key(X509_ROOT "/keys/rsa2048.pem");
	assert_not_null("2048-bit RSA key retrieval", key);
	assert_int_eq("rsa2048.pem should be an RSA key", key->type, EVP_PKEY_RSA);
	assert_int_eq("rsa2048.pem should be 2048-bit", RSA_size(key->pkey.rsa) * 8, 2048);
	EVP_PKEY_free(key);
}

void test_cert_key_generation()
{
	EVP_PKEY *key;

	test("Cert: Private Key generation");
	key = cert_generate_key(512);
	assert_not_null("Key generated successfully", key);
	assert_int_eq("Key is RSA", key->type, EVP_PKEY_RSA);
	assert_int_eq("Key is 512-bit strength", RSA_size(key->pkey.rsa) * 8, 512);
	EVP_PKEY_free(key);
}

void test_cert_key_storage()
{
	EVP_PKEY *key, *reread;
	const char *path = TMPROOT "/x509/key1.pem";
	const char *badpath = "/path/to/nonexistent/file.pem";

	test("Cert: Private Key storage");

	key = cert_retrieve_key(X509_ROOT "/keys/rsa2048.pem");
	assert_not_null("Key Retrieval (prior to store and check)", key);

	assert_int_eq("Storing key in temp file succeeds", cert_store_key(key, path), 0);
	reread = cert_retrieve_key(path);
	assert_not_null("Key Retrieval (after store)", reread);

	test("Cert: Private Key storage w/ bad destination");

	assert_int_ne("Storing key in bad path fails", cert_store_key(key, badpath), 0);

	EVP_PKEY_free(key);
	EVP_PKEY_free(reread);
}

void test_cert_csr_retrieve()
{
	X509_REQ *request;
	const char *path = X509_ROOT "/csrs/csr.pem";
	const char *badpath = "/path/to/nonexistent/csr.pem";
	char *subject_name;
	const char *expected = "NiftyLogic - csr.niftylogic.net";

	test("Cert: CSR retrieve");

	request = cert_retrieve_request(badpath);
	assert_null("Retrieve fails for bad path", request);
	X509_REQ_free(request);

	request = cert_retrieve_request(path);
	assert_not_null("Retrieve succeeds for valid path", request);

	subject_name = cert_request_subject_name(request);
	assert_not_null("CSR has a subject name", subject_name);
	assert_str_eq("CSR subject name succeeds", expected, subject_name);
	free(subject_name);
	X509_REQ_free(request);
}

void test_cert_csr_generation()
{
	EVP_PKEY *key;
	X509_REQ *request;
	char *subject_name;
	const char *expected = "Testing, LLC - c.example.net";
	struct cert_subject subj = {
		.country  = "US",
		.state    = "Illinois",
		.loc      = "Peoria",
		.org      = "Testing, LLC",
		.org_unit = "SysDev",
		.type     = "Clockwork Agent",
		.fqdn     = "c.example.net"
	};

	key = cert_retrieve_key(X509_ROOT "/keys/rsa2048.pem");
	assert_not_null("Key for CSR generation is not null", key);

	request = cert_generate_request(key, &subj);
	assert_not_null("Request generation succeeds", request);

	subject_name = cert_request_subject_name(request);
	assert_not_null("Generated request has a subject name", subject_name);
	assert_str_eq("CSR subject name succeeds", expected, subject_name);
	free(subject_name);

	EVP_PKEY_free(key);
	X509_REQ_free(request);
}

void test_cert_csr_storage()
{
	X509_REQ *request, *reread;
	const char *path = TMPROOT "/x509/csr1.pem";
	const char *badpath = "/path/to/nonexistent/file.pem";

	test("Cert: CSR storage");

	request = cert_retrieve_request(X509_ROOT "/csrs/csr.pem");
	assert_not_null("CSR retrieval (prior to store and check)", request);

	assert_int_eq("Storing CSR in temp file succeeds", cert_store_request(request, path), 0);
	reread = cert_retrieve_request(path);
	assert_not_null("CSR retrieval (after store)", reread);

	test("Cert: CSR storage w/ bad destination");

	assert_int_ne("Storing CSR in a bad path fails", cert_store_request(request, badpath), 0);
	X509_REQ_free(request);
	X509_REQ_free(reread);
}

void test_cert_info()
{
	X509 *cert;
	char *issuer;
	char *subject;
	char *serial;
	char *fingerprint;

	test("cert: Certificate Information");
	cert = cert_retrieve_certificate(X509_ROOT "/certs/test.pem");
	assert_not_null("Certificate retrieval succeeded", cert);

	issuer = cert_certificate_issuer_name(cert);
	assert_not_null("Certificate Issuer retrieved", issuer);
	assert_str_eq("Cert Issuer is correct", issuer, CA_ID);

	subject = cert_certificate_subject_name(cert);
	assert_not_null("Certificate Subject retrieved", subject);
	assert_str_eq("Cert Subject is correct", subject,
		"NiftyLogic - signed.niftylogic.net");

	serial = cert_certificate_serial_number(cert);
	assert_not_null("Certificate Serial # retrieved", serial);
	assert_str_eq("Cert Serial # is correct", serial, "123456");

	fingerprint = cert_fingerprint_certificate(cert);
	assert_not_null("Certificate Fingerprint retrieved", fingerprint);
	assert_str_eq("Cert Fingerprint is correct", fingerprint, FPRINT_TEST_CERT);

	free(issuer);
	free(subject);
	free(serial);
	free(fingerprint);
}

void test_cert_signing()
{
	X509_REQ *csr;
	X509 *cert;
	X509 *ca_cert;
	EVP_PKEY *ca_key;
	const char *cert_file = X509_ROOT "/certs/sign-me.pem";

	test("cert: Certificate Signing");
	ca_cert = cert_retrieve_certificate(CA_CERT);
	assert_not_null("Retrieved CA certificate", ca_cert);

	ca_key = cert_retrieve_key(CA_KEY);
	assert_not_null("Retrieved CA igning key", ca_key);

	csr = cert_retrieve_request(X509_ROOT "/csrs/sign-me.pem");
	assert_not_null("Retrieved Cert Request", csr);

	cert = cert_sign_request(csr, ca_cert, ca_key, 2);
	assert_not_null("Cert Signed", cert);

	assert_cert_id(ca_cert, CA_ID, CA_ID);
	assert_cert_id(cert, CA_ID, "Signing Test - sign.niftylogic.net");

	test("cert: Certificate Storage / Retrieval");
	assert_int_eq("Certificate stored successfully",
		cert_store_certificate(cert, cert_file), 0);

	cert = cert_retrieve_certificate(cert_file);
	assert_not_null("Certificate retrieved successfully", cert);
	assert_cert_id(cert, CA_ID, "Signing Test - sign.niftylogic.net");
}

void test_cert_revocation()
{
	X509_CRL *crl;
	EVP_PKEY *ca_key;
	X509 *ca_cert;
	X509 *cert;

	test("cert: Certificate Revocation List Creation");
	ca_cert = cert_retrieve_certificate(CA_CERT);
	assert_not_null("Retrieved CA certificate", ca_cert);

	ca_key = cert_retrieve_key(CA_KEY);
	assert_not_null("Retrieved CA igning key", ca_key);

	cert = cert_retrieve_certificate(X509_ROOT "/certs/revoke-me.pem");
	assert_not_null("Retrieved Certificate to revoke", cert);

	crl = cert_generate_crl(ca_cert);
	assert_not_null("Generated blank CRL", crl);
	assert_int_ne("Cert not already revoked (blank CRL)",
		cert_is_revoked(crl, cert), 0);

	test("cert: Revoke Certificate");
	assert_int_eq("Revoking Certificate succeeds",
		cert_revoke_certificate(crl, cert, ca_key), 0);
	assert_int_eq("Cert is now revoked", cert_is_revoked(crl, cert), 0);
	assert_int_ge("Double-revoke fails",
		cert_revoke_certificate(crl, cert, ca_key), 1);
	assert_int_eq("Cert is still revoked", cert_is_revoked(crl, cert), 0);

	test("cert: Store and Retrieved Certificate");
	assert_int_eq("Storing CRL succeeds", cert_store_crl(crl, CA_CRL), 0);
	crl = cert_retrieve_crl(CA_CRL);
	assert_not_null("Re-reading CRL succeeds", crl);
	assert_int_eq("Cert is still revoked after re-read", cert_is_revoked(crl, cert), 0);
}

void test_cert_retrieve_multi()
{
	X509_REQ **reqs;
	X509 **certs;
	int n = 0;

	test("cert: Retrieve All Certificate Requests");
	reqs = cert_retrieve_requests(X509_ROOT "/csrs/*.pem", &n);
	assert_not_null("Retrieved requests", reqs);
	assert_int_gt("Found multiple cert requests", n, 0);

	reqs = cert_retrieve_requests(X509_ROOT "/*.csr", &n);
	assert_null("Request retrieval fails for bad path", reqs);
	assert_int_eq("No requests found matching bad path", n, 0);

	test("cert: Retrieve All Certificates");
	certs = cert_retrieve_certificates(X509_ROOT "/certs/*.pem", &n);
	assert_not_null("Retrieved certs", certs);
	assert_int_gt("Found multiple certs", n, 0);

	certs = cert_retrieve_certificates(X509_ROOT "/*.cert", &n);
	assert_null("Certificate retrieval fails for bad path", certs);
	assert_int_eq("No certs found matching bad path", n, 0);
}

void test_suite_cert()
{
	cert_init();

	test_cert_info();

	test_cert_key_retrieve();
	test_cert_key_generation();
	test_cert_key_storage();

	test_cert_csr_retrieve();
	test_cert_csr_generation();
	test_cert_csr_storage();

	test_cert_signing();
	test_cert_revocation();

	test_cert_retrieve_multi();

	cert_deinit();
}
