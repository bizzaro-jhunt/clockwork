#include "test.h"
#include "assertions.h"
#include "../../cert.h"

void test_cert_key_retrieve()
{
	EVP_PKEY *key;

	test("Cert: Private Key retrieval");

	key = cert_retrieve_key(DATAROOT "/x509/keys/rsa128.pem");
	assert_not_null("128-bit RSA key retrieval", key);
	assert_int_eq("rsa128.pem should be an RSA key", key->type, EVP_PKEY_RSA);
	assert_int_eq("rsa128.pem should be 128-bit", RSA_size(key->pkey.rsa) * 8, 128);
	EVP_PKEY_free(key);

	key = cert_retrieve_key(DATAROOT "/x509/keys/rsa1024.pem");
	assert_not_null("1024-bit RSA key retrieval", key);
	assert_int_eq("rsa1024.pem should be an RSA key", key->type, EVP_PKEY_RSA);
	assert_int_eq("rsa1024.pem should be 1024-bit", RSA_size(key->pkey.rsa) * 8, 1024);
	EVP_PKEY_free(key);

	key = cert_retrieve_key(DATAROOT "/x509/keys/rsa2048.pem");
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

	key = cert_retrieve_key(DATAROOT "/x509/keys/rsa2048.pem");
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
	const char *path = DATAROOT "/x509/csrs/request.pem";
	const char *badpath = "/path/to/nonexistent/csr.pem";
	char *subject_name;
	const char *expected = "NiftyLogic - test.rd.niftylogic.net";

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

	key = cert_retrieve_key(DATAROOT "/x509/keys/rsa2048.pem");
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

	request = cert_retrieve_request(DATAROOT "/x509/csrs/request.pem");
	assert_not_null("CSR retrieval (prior to store and check)", request);

	assert_int_eq("Storing CSR in temp file succeeds", cert_store_request(request, path), 0);
	reread = cert_retrieve_request(path);
	assert_not_null("CSR retrieval (after store)", reread);

	test("Cert: CSR storage w/ bad destination");

	assert_int_ne("Storing CSR in a bad path fails", cert_store_request(request, badpath), 0);
	X509_REQ_free(request);
	X509_REQ_free(reread);
}

void test_cert_cert_retrieve()
{
}

void test_cert_crl_retrieve()
{
}

void test_suite_cert()
{
	cert_init();

	test_cert_key_retrieve();
	test_cert_key_generation();
	test_cert_key_storage();

	test_cert_csr_retrieve();
	test_cert_csr_generation();
	test_cert_csr_storage();

	test_cert_cert_retrieve();
	test_cert_crl_retrieve();

	cert_deinit();
}
