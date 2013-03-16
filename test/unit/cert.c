/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

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

#include "test.h"
#include "../../cert.h"

#define X509_ROOT  TEST_UNIT_DATA "/x509"
#define CA_KEY     X509_ROOT "/ca/key.pem"
#define CA_CERT    X509_ROOT "/ca/cert.pem"
#define CA_CRL     TEST_UNIT_TEMP "/crl.pem"

#define CA_ID "Clockwork Root CA - cfm.niftylogic.net"

static void assert_cert_id(X509 *cert, const char *issuer, const char *subject)
{
	struct cert_subject *subj;
	char *str;

	assert_not_null("cert to validate ID for is not NULL", cert);
	assert_not_null("issuer specified in call to assert_cert_id", issuer);
	assert_not_null("subject specified in call to assert_cert_id", subject);

	subj = cert_certificate_issuer(cert);
	assert_not_null("cert issuer is present", subj);
	str = string("%s - %s", subj->org, subj->fqdn);
	assert_str_eq("verify certificate issuer", str, issuer);
	free(str);
	cert_subject_free(subj);

	subj = cert_certificate_subject(cert);
	assert_not_null("cert subject is present", subj);
	str = string("%s - %s", subj->org, subj->fqdn);
	assert_str_eq("verify certificate subject", str, subject);
	free(str);
	cert_subject_free(subj);
}

NEW_TEST(cert_key_retrieve)
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

NEW_TEST(cert_key_generation)
{
	EVP_PKEY *key;

	test("Cert: Private Key generation");
	key = cert_generate_key(512);
	assert_not_null("Key generated successfully", key);
	assert_int_eq("Key is RSA", key->type, EVP_PKEY_RSA);
	assert_int_eq("Key is 512-bit strength", RSA_size(key->pkey.rsa) * 8, 512);
	EVP_PKEY_free(key);
}

NEW_TEST(cert_key_storage)
{
	EVP_PKEY *key, *reread;
	const char *path = TEST_UNIT_TEMP "/x509/key1.pem";
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

NEW_TEST(cert_csr_retrieve)
{
	X509_REQ *request;
	struct cert_subject *subj;

	const char *path = X509_ROOT "/csrs/csr.pem";
	const char *badpath = "/path/to/nonexistent/csr.pem";

	test("Cert: CSR retrieve");

	request = cert_retrieve_request(badpath);
	assert_null("Retrieve fails for bad path", request);
	X509_REQ_free(request);

	request = cert_retrieve_request(path);
	assert_not_null("Retrieve succeeds for valid path", request);

	subj = cert_request_subject(request);
	assert_not_null("CSR has a subject", subj);
	assert_str_eq("CSR Subject // C",    subj->country,  "US");
	assert_str_eq("CSR Subject // ST",   subj->state,    "Illinois");
	assert_str_eq("CSR Subject // L",    subj->loc,      "Peoria");
	assert_str_eq("CSR Subject // O",    subj->org,      "NiftyLogic");
	assert_str_eq("CSR Subject // OU",   subj->org_unit, "R&D");
	assert_str_eq("CSR Subject // type", subj->type,     "CWA");
	cert_subject_free(subj);

	X509_REQ_free(request);
}

NEW_TEST(cert_csr_generation)
{
	EVP_PKEY *key;
	X509_REQ *request;
	struct cert_subject *subj;

	struct cert_subject build_subject = {
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

	request = cert_generate_request(key, &build_subject);
	assert_not_null("Request generation succeeds", request);

	subj = cert_request_subject(request);
	assert_not_null("CSR has a subject", subj);
	assert_str_eq("CSR Subject // C",    subj->country,  "US");
	assert_str_eq("CSR Subject // ST",   subj->state,    "Illinois");
	assert_str_eq("CSR Subject // L",    subj->loc,      "Peoria");
	assert_str_eq("CSR Subject // O",    subj->org,      "Testing, LLC");
	assert_str_eq("CSR Subject // OU",   subj->org_unit, "SysDev");
	assert_str_eq("CSR Subject // type", subj->type,     "Clockwork Agent");
	assert_str_eq("CSR Subject // CN",   subj->fqdn,     "c.example.net");
	cert_subject_free(subj);

	EVP_PKEY_free(key);
	X509_REQ_free(request);
}

NEW_TEST(cert_csr_storage)
{
	X509_REQ *request, *reread;
	const char *path = TEST_UNIT_TEMP "/x509/csr1.pem";
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

NEW_TEST(cert_free_null)
{
	test("cert: cert_subject_free(NULL)");
	cert_subject_free(NULL);
	assert_pass("cert_subject_free(NULL) doesn't segfault");
}

NEW_TEST(cert_info)
{
	X509 *cert;
	char *serial;
	char *fingerprint;
	struct cert_subject *subj;

	test("cert: Certificate Information");
	cert = cert_retrieve_certificate(X509_ROOT "/certs/test.pem");
	assert_not_null("Certificate retrieval succeeded", cert);

	subj = cert_certificate_issuer(cert);
	assert_not_null("Certificate Issuer object retrieved", subj);
	assert_str_eq("Cert Issuer // C",    subj->country, "US");
	assert_str_eq("Cert Issuer // ST",   subj->state,   "Illinois");
	assert_str_eq("Cert Issuer // L",    subj->loc,     "Peoria");
	assert_str_eq("Cert Issuer // O",    subj->org,     "Clockwork Root CA");
	assert_str_eq("Cert Issuer // type", subj->type,    "Policy Master");
	assert_str_eq("Cert Issuer // CN",   subj->fqdn,    "cfm.niftylogic.net");
	assert_null("Cert Issuer // OU is NULL", subj->org_unit);
	cert_subject_free(subj);

	subj = cert_certificate_subject(cert);
	assert_not_null("Certificate Subject object retrieved", subj);
	assert_str_eq("Cert Subject // C",    subj->country, "US");
	assert_str_eq("Cert Subject // ST",   subj->state,   "Illinois");
	assert_str_eq("Cert Subject // L",    subj->loc,     "Peoria");
	assert_str_eq("Cert Subject // O",    subj->org,     "NiftyLogic");
	assert_str_eq("Cert Subject // type", subj->type,    "CWA");
	assert_str_eq("Cert Subject // CN",   subj->fqdn,    "signed.niftylogic.net");
	assert_null("Cert Subject // OU is NULL", subj->org_unit);
	cert_subject_free(subj);

	serial = cert_certificate_serial_number(cert);
	assert_not_null("Certificate Serial # retrieved", serial);
	assert_str_eq("Cert Serial # is correct", serial, "123456");
	free(serial);

	fingerprint = cert_fingerprint_certificate(cert);
	assert_not_null("Certificate Fingerprint retrieved", fingerprint);
	assert_str_eq("Cert Fingerprint is correct", fingerprint, FPRINT_TEST_CERT);
	free(fingerprint);
}

NEW_TEST(cert_signing)
{
	X509_REQ *csr;
	X509 *cert;
	X509 *ca_cert;
	EVP_PKEY *ca_key;
	const char *cert_file = TEST_UNIT_TEMP "/x509/sign-me.pem";

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

NEW_TEST(cert_revocation)
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

NEW_TEST(cert_retrieve_multi)
{
	X509_REQ **reqs;
	X509 **certs;
	size_t n = 0;

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

NEW_SUITE(cert)
{
	cert_init();

	RUN_TEST(cert_free_null);
	RUN_TEST(cert_info);

	RUN_TEST(cert_key_retrieve);
	RUN_TEST(cert_key_generation);
	RUN_TEST(cert_key_storage);

	RUN_TEST(cert_csr_retrieve);
	RUN_TEST(cert_csr_generation);
	RUN_TEST(cert_csr_storage);

	RUN_TEST(cert_signing);
	RUN_TEST(cert_revocation);

	RUN_TEST(cert_retrieve_multi);

	cert_deinit();
}
