/*
  Copyright 2011-2014 James Hunt <james@niftylogic.com>

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
#include "../src/cert.h"

TESTS {
	cert_init();

	subtest {
		cert_subject_free(NULL);
		pass("cert_subject_free(NULL) doesn't segfault");
	}

	subtest {
		X509 *cert;
		char *serial;
		char *fingerprint;
		struct cert_subject *subj;

		isnt_null(cert = cert_retrieve_certificate("t/data/x509/certs/test.pem"),
				"retrieved certificate");
		isnt_null(subj = cert_certificate_issuer(cert), "got cert. issuer");
		is_string(subj->country,  "US",                 "cert issuer C=");
		is_string(subj->state,    "New York",           "cert issuer ST=");
		is_string(subj->loc,      "Buffalo",            "cert issuer L=");
		is_string(subj->org,      "Clockwork Root CA",  "cert issuer O=");
		is_string(subj->type,     "Policy Master",      "cert issuer type");
		is_string(subj->fqdn,     "cfm.niftylogic.net", "cert issuer CN=");
		is_null(subj->org_unit, "cert issuer has no OU");
		cert_subject_free(subj);

		isnt_null(subj = cert_certificate_subject(cert), "got cert. subject");
		is_string(subj->country,  "US",                    "cert C=");
		is_string(subj->state,    "New York",              "cert ST=");
		is_string(subj->loc,      "Buffalo",               "cert L=");
		is_string(subj->org,      "NiftyLogic",            "cert O=");
		is_string(subj->type,     "CWA",                   "cert type");
		is_string(subj->fqdn,     "signed.niftylogic.net", "cert CN=");
		is_string(subj->org_unit, "R&D",                   "cert OU=");
		cert_subject_free(subj);

		isnt_null(serial = cert_certificate_serial_number(cert), "got cert. serial");
		is_string(serial, "123456", "cert serial");
		free(serial);

		isnt_null(fingerprint = cert_fingerprint_certificate(cert), "got cert fingerprint");
		is_string(fingerprint,
			"02:f8:40:c7:0d:a5:50:e7:3f:db:82:89:10:6d:a0:ab:a5:20:2b:ae",
			"cert fingerprint");
		free(fingerprint);
	}

	subtest {
		EVP_PKEY *key;

		isnt_null(key = cert_retrieve_key("t/data/x509/keys/rsa128.pem"), "retrieved 128-bit RSA key");
		is_int(key->type, EVP_PKEY_RSA, "rsa128.pem is an RSA key");
		is_int(RSA_size(key->pkey.rsa) * 8, 128, "rsa128.pem is a 128-bit key");
		EVP_PKEY_free(key);

		isnt_null(key = cert_retrieve_key("t/data/x509/keys/rsa1024.pem"), "retrieved 1024-bit RSA key");
		is_int(key->type, EVP_PKEY_RSA, "rsa1024.pem is an RSA key");
		is_int(RSA_size(key->pkey.rsa) * 8, 1024, "rsa1024.pem is a 1024-bit key");
		EVP_PKEY_free(key);

		isnt_null(key = cert_retrieve_key("t/data/x509/keys/rsa2048.pem"), "retrieved 2048-bit RSA key");
		is_int(key->type, EVP_PKEY_RSA, "rsa2048.pem is an RSA key");
		is_int(RSA_size(key->pkey.rsa) * 8, 2048, "rsa2048.pem is a 2048-bit key");
		EVP_PKEY_free(key);
	}

	subtest {
		EVP_PKEY *key;

		isnt_null(key = cert_generate_key(512), "generated key");
		is_int(key->type, EVP_PKEY_RSA, "generated RSA key");
		is_int(RSA_size(key->pkey.rsa) * 8, 512, "generated a 512-bit key");
		EVP_PKEY_free(key);
	}

	subtest {
		EVP_PKEY *key, *reread;

		isnt_null(key = cert_retrieve_key("t/data/x509/keys/rsa2048.pem"),
				"retrieved key (prior to store and check)");

		ok(cert_store_key(key, "/no/such/path") != 0, "store with bad path");
		ok(cert_store_key(key, "t/tmp/key1.pem") == 0, "stored key in temp file");
		isnt_null(reread = cert_retrieve_key("t/tmp/key1.pem"), "re-retrieved key");

		EVP_PKEY_free(key);
		EVP_PKEY_free(reread);
	}


	subtest {
		X509_REQ *request;
		struct cert_subject *subj;

		is_null(cert_retrieve_request("/no/such/path"), "retrieve CSR bad path");
		isnt_null(request = cert_retrieve_request("t/data/x509/csrs/csr.pem"),
				"retrieved cert signing request");

		isnt_null(subj = cert_request_subject(request), "got CSR subject");
		is_string(subj->country,  "US",         "csr C=");
		is_string(subj->state,    "New York",   "csr ST=");
		is_string(subj->loc,      "Buffalo",    "csr L=");
		is_string(subj->org,      "NiftyLogic", "csr O=");
		is_string(subj->org_unit, "R&D",        "csr OU=");
		is_string(subj->type,     "CWA",        "csr type");

		cert_subject_free(subj);
		X509_REQ_free(request);
	}

	subtest {
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

		isnt_null(key = cert_retrieve_key("t/data/x509/keys/rsa2048.pem"),
			"retrieved key for CSR generation");
		isnt_null(request = cert_generate_request(key, &build_subject),
			"generated a certificate signing request");

		isnt_null(subj = cert_request_subject(request), "CSR has a subject");
		is_string(subj->country,  "US",              "csr C=");
		is_string(subj->state,    "Illinois",        "csr ST=");
		is_string(subj->loc,      "Peoria",          "csr L=");
		is_string(subj->org,      "Testing, LLC",    "csr O=");
		is_string(subj->org_unit, "SysDev",          "csr OU=");
		is_string(subj->type,     "Clockwork Agent", "csr type");
		is_string(subj->fqdn,     "c.example.net",   "csr CN=");

		cert_subject_free(subj);
		EVP_PKEY_free(key);
		X509_REQ_free(request);
	}

	subtest {
		X509_REQ *request, *reread;

		isnt_null(request = cert_retrieve_request("t/data/x509/csrs/csr.pem"),
			"Retrieved CSR (prior to store/check)");
		ok(cert_store_request(request, "no/such/path") != 0,
			"CSR storage fails with bad path");
		ok(cert_store_request(request, "t/tmp/csr1.pem") == 0,
			"stored CSR into temp file");
		isnt_null(reread = cert_retrieve_request("t/tmp/csr1.pem"),
			"reread CSR from temp file");

		X509_REQ_free(request);
		X509_REQ_free(reread);
	}


	subtest {
		X509_REQ *csr;
		X509 *cert, *ca_cert;
		EVP_PKEY *ca_key;
		struct cert_subject *subj;

		isnt_null(ca_cert = cert_retrieve_certificate("t/data/x509/ca/cert.pem"),
			"retrieved CA certificate");
		isnt_null(ca_key = cert_retrieve_key("t/data/x509/ca/key.pem"),
			"retrieved CA signing key");
		isnt_null(csr = cert_retrieve_request("t/data/x509/csrs/sign-me.pem"),
			"retrieved certificate signing request");

		isnt_null(cert = cert_sign_request(csr, ca_cert, ca_key, 2), "signed");

		isnt_null(subj = cert_certificate_issuer(ca_cert), "cert issuer found");
		is_string(subj->org,  "Clockwork Root CA",  "CA issuer O=");
		is_string(subj->fqdn, "cfm.niftylogic.net", "CA issuer CN=");
		cert_subject_free(subj);

		isnt_null(subj = cert_certificate_subject(ca_cert), "cert subject found");
		is_string(subj->org,  "Clockwork Root CA",  "CA cert O=");
		is_string(subj->fqdn, "cfm.niftylogic.net", "CA cert CN=");
		cert_subject_free(subj);


		isnt_null(subj = cert_certificate_issuer(cert), "cert issuer found");
		is_string(subj->org,  "Clockwork Root CA",  "issuer O=");
		is_string(subj->fqdn, "cfm.niftylogic.net", "issuer CN=");
		cert_subject_free(subj);

		isnt_null(subj = cert_certificate_subject(cert), "cert subject found");
		is_string(subj->org,  "Signing Test",        "cert O=");
		is_string(subj->fqdn, "sign.niftylogic.net", "cert CN=");
		cert_subject_free(subj);

		ok(cert_store_certificate(cert, "t/tmp/signed.pem") == 0,
			"certificate stored in temp file");

		isnt_null(cert = cert_retrieve_certificate("t/tmp/signed.pem"),
			"re-read signed certificate");
		isnt_null(subj = cert_certificate_issuer(cert), "cert issuer found");
		is_string(subj->org,  "Clockwork Root CA",  "issuer O=");
		is_string(subj->fqdn, "cfm.niftylogic.net", "issuer CN=");
		cert_subject_free(subj);

		isnt_null(subj = cert_certificate_subject(cert), "cert subject found");
		is_string(subj->org,  "Signing Test",        "cert O=");
		is_string(subj->fqdn, "sign.niftylogic.net", "cert CN=");
		cert_subject_free(subj);
	}

	subtest {
		X509_CRL *crl;
		EVP_PKEY *ca_key;
		X509 *cert, *ca_cert;

		isnt_null(ca_cert = cert_retrieve_certificate("t/data/x509/ca/cert.pem"), "read CA cert");
		isnt_null(ca_key = cert_retrieve_key("t/data/x509/ca/key.pem"), "read signing key");
		isnt_null(cert = cert_retrieve_certificate("t/data/x509/certs/revoke-me.pem"), "read cert");

		isnt_null(crl = cert_generate_crl(ca_cert), "generated CRL");
		ok(cert_is_revoked(crl, cert) != 0, "cert not already revoked");

		ok(cert_revoke_certificate(crl, cert, ca_key) == 0, "revoked");
		ok(cert_is_revoked(crl, cert) == 0, "CRL lists cert as revoked");
		ok(cert_revoke_certificate(crl, cert, ca_key) != 0, "double-revoke fails");
		ok(cert_is_revoked(crl, cert) == 0, "CRL still lists cert as revoked");

		ok(cert_store_crl(crl, "t/tmp/crl.pem") == 0, "wrote CRL to temp file");
		isnt_null(crl = cert_retrieve_crl("t/tmp/crl.pem"), "retrieved CRL from temp file");
		ok(cert_is_revoked(crl, cert) == 0, "CRL still lists cert as revoked");
	}

	subtest {
		X509_REQ **reqs;
		X509 **certs;
		size_t n = 0;

		isnt_null(reqs = cert_retrieve_requests("t/data/x509/csrs/*.pem", &n), "read CSRs");
		ok(n > 0, "found multiple cert requests");

		is_null(cert_retrieve_requests("t/data/x509/*.csr", &n), "no requests (bad path)");
		is_int(n, 0, "no requests (bad path)");

		isnt_null(certs = cert_retrieve_certificates("t/data/x509/certs/*.pem", &n), "read certs");
		ok(n > 0, "found multiple certs");

		is_null(cert_retrieve_certificates("t/data/x509/*.cert", &n), "no certs (bad path)");
		is_int(n, 0, "no certs(bad path)");
	}

	cert_deinit();
	done_testing();
}
