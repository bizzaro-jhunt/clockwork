/*
  Copyright 2011 James Hunt <james@jameshunt.us>

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

#include "../../proto.h"
#include "../../cert.h"

#include <sys/types.h>
#include <sys/wait.h>

#define assert_op_name(op) assert_str_eq("Op name for " #op, protocol_op_name(PROTOCOL_OP_ ## op), #op)

#define SUB  "./test/unit/helpers/proto_helper"
#define ARG0 "proto_helper"

struct test_proto_conn {
	pid_t             pid;
	SSL_CTX          *ctx;
	SSL              *ssl;
	struct session  session;
};

/* In order to still operate under the SSL model that the proto
   module requires, but without the need for server and client
   certs, we have to use anonymous Diffie-Helman (anon-DH) mode.

   THIS IS NOT SECURE!

   Then again,

   THIS IS A TEST!

   Therefore, the DH support is statically coded in this test
   suite, and not available to the rest of the world. */
static int test_proto_conn_server(struct test_proto_conn *server, int r, int w)
{
	BIO *rbio = NULL, *wbio = NULL;
	DH *dh = NULL;
	int ignored;

	server->ctx = SSL_CTX_new(TLSv1_method());
	assert_not_null("SSL context is not null (server)", server->ctx);
	/* this failed for REPORT; maybe we hit our limit? */
	if (!server->ctx) { protocol_ssl_backtrace(); }
	if (!server->ctx) { return -1; }
	if (SSL_CTX_set_cipher_list(server->ctx, "ADH") != 1) { return -2; }

	dh = DH_generate_parameters(16, 2, NULL, NULL);
	if (!DH_check(dh, &ignored)) { return -3; }
	if (!DH_generate_key(dh)) { return -4; }

	SSL_CTX_set_tmp_dh(server->ctx, dh);

	server->ssl = SSL_new(server->ctx);
	assert_not_null("SSL handle is not null (server)", server->ssl);
	if (!server->ssl) { return -3; }

	rbio = BIO_new_fd(r, BIO_CLOSE);
	wbio = BIO_new_fd(w, BIO_CLOSE);

	assert_not_null("Read BIO is not null (server)", rbio);
	assert_not_null("Write BIO is not null (server)", rbio);

	SSL_set_bio(server->ssl, rbio, wbio);
	return 0;
}

static void test_proto_conn_free(struct test_proto_conn *c)
{
	SSL_free(c->ssl);
	SSL_CTX_free(c->ctx);
}


typedef void (*child_fn)(void);

static pid_t fork_test(struct test_proto_conn *server, child_fn test)
{
	pid_t pid;
	int rc;
	int s2c[2], c2s[2];

	assert_int_eq("s2c pipe created successfully", pipe(s2c), 0);
	assert_int_eq("c2s pipe created successfully", pipe(c2s), 0);

	pid = fork();

	if (pid == 0) {
		close(s2c[1]);
		close(c2s[0]);

		dup2(s2c[0], 0);
		dup2(c2s[1], 1);

		(*test)();
		CRITICAL("fork_test: exec in child failed: %s", strerror(errno));
		exit(127);

	} else {
		assert_int_ge("forked successfully", pid, 0);
		close(c2s[1]);
		close(s2c[0]);

		assert_int_eq("Server test connection setup properly",
			test_proto_conn_server(server, c2s[0], s2c[1]), 0);

		SSL_accept(server->ssl);
		protocol_session_init(&server->session, server->ssl);

		return pid;
	}
}

static void waitfork(pid_t pid, struct test_proto_conn *conn)
{
	int status;
	waitpid(pid, &status, 0);
	assert_int_ne("child terminated normally", WIFEXITED(status), 0);
	assert_int_eq("child exited zero (0)", WEXITSTATUS(status), 0);
	test_proto_conn_free(conn);
}

NEW_TEST(proto_op_names)
{
	test("proto: PDU op names");
	assert_op_name(ERROR);
	assert_op_name(HELLO);
	assert_op_name(FACTS);
	assert_op_name(POLICY);
	assert_op_name(FILE);
	assert_op_name(DATA);
	assert_op_name(REPORT);
	assert_op_name(BYE);
	assert_op_name(GET_CERT);
	assert_op_name(SEND_CERT);

	assert_null("Unknown protocol ops are NULL", protocol_op_name(9999999));
}

static void child_expects_BYE(void) { execl(SUB, ARG0, "BYE", NULL); }
NEW_TEST(proto_pdu_BYE)
{
	pid_t pid;
	struct test_proto_conn server;

	test("proto: BYE PDU");
	pid = fork_test(&server, child_expects_BYE);
	assert_int_eq("pdu_send_BYE returns successfully", pdu_send_BYE(&server.session), 0);
	waitfork(pid, &server);

	protocol_session_deinit(&server.session);
}

static void child_expects_ERROR(void) { execl(SUB, ARG0, "ERROR", NULL); }
NEW_TEST(proto_pdu_ERROR)
{
	pid_t pid;
	struct test_proto_conn server;

	test("proto: ERROR PDU");
	pid = fork_test(&server, child_expects_ERROR);
	assert_int_eq("pdu_send_ERROR returns successfully",
		pdu_send_ERROR(&server.session, 404, "not found"), 0);
	waitfork(pid, &server);

	protocol_session_deinit(&server.session);
}

static void child_expects_FACTS(void) { execl(SUB, ARG0, "FACTS", NULL); }
NEW_TEST(proto_pdu_FACTS)
{
	struct hash *facts;
	pid_t pid;
	struct test_proto_conn server;

	test("proto: FACTS PDU");

	facts = hash_new();
	hash_set(facts, "fact1", "value1");
	hash_set(facts, "fact2", "v2");

	pid = fork_test(&server, child_expects_FACTS);
	assert_int_eq("pdu_send_FACTS returns successfully",
		pdu_send_FACTS(&server.session, facts), 0);
	waitfork(pid, &server);

	protocol_session_deinit(&server.session);
	hash_free(facts);
}

static void child_expects_POLICY(void) { execl(SUB, ARG0, "POLICY", NULL); }
NEW_TEST(proto_pdu_POLICY)
{
	struct policy *pol;
	pid_t pid;
	struct test_proto_conn server;

	test("proto: POLICY PDU");

	pol = policy_new("testpolicy");
	pid = fork_test(&server, child_expects_POLICY);
	assert_int_eq("pdu_send_POLICY return successfully",
		pdu_send_POLICY(&server.session, pol), 0);
	waitfork(pid, &server);

	protocol_session_deinit(&server.session);
	policy_free(pol);
}

#define PDU_SHA1 "78745d280fea5e30dbaf54e1824a0ecc89e4b505"
static void child_expects_FILE(void) { execl(SUB, ARG0, "FILE", PDU_SHA1, NULL); }
NEW_TEST(proto_pdu_FILE)
{
	struct SHA1 checksum;
	pid_t pid;
	struct test_proto_conn server;

	test("proto: FILE PDU");

	sha1_init(&checksum, PDU_SHA1);
	pid = fork_test(&server, child_expects_FILE);
	assert_int_eq("pdu_send_FILE returns successfully",
		pdu_send_FILE(&server.session, &checksum), 0);
	waitfork(pid, &server);

	protocol_session_deinit(&server.session);
}
#undef PDU_SHA1

static void child_expects_DATA(void) { execl(SUB, ARG0, "DATA", NULL); }
NEW_TEST(proto_pdu_DATA)
{
	int fd[2];
	pid_t pid;
	struct test_proto_conn server;

	test("proto: DATA PDU");

	assert_int_eq("pipe created successfully", pipe(fd), 0);
	write(fd[1], "xyzy", 4);
	close(fd[1]);

	pid = fork_test(&server, child_expects_DATA);
	assert_int_ne("first pdu_send_DATA returns non-zero",
		pdu_send_DATA(&server.session, fd[0], NULL), 0);
	assert_int_eq("second pdu_send_DATA returns zero",
		pdu_send_DATA(&server.session, fd[0], NULL), 0);
	close(fd[0]);
	waitfork(pid, &server);

	protocol_session_deinit(&server.session);
}

static void child_expects_GET_CERT(void) { execl(SUB, ARG0, "GET_CERT", NULL); }
NEW_TEST(proto_pdu_GET_CERT)
{
	pid_t pid;
	struct test_proto_conn server;

	X509_REQ *req;
	EVP_PKEY *key;
	struct cert_subject subj = {
		.fqdn     = "test1.example.net",
		.country  = "US",
		.loc      = "Peoria",
		.state    = "IL",
		.org      = "NiftyLogic",
		.type     = "Agent",
		.org_unit = "R&D"
	};

	test("proto: GET_CERT PDU");

	key = cert_generate_key(2048);
	assert_not_null("Have a valid key", key);

	req = cert_generate_request(key, &subj);
	assert_not_null("Have a valid CSR", req);

	pid = fork_test(&server, child_expects_GET_CERT);
	assert_int_ne("pdu_send_GET_CERT returns successfully",
		pdu_send_GET_CERT(&server.session, req), 0);
	waitfork(pid, &server);

	protocol_session_deinit(&server.session);
}

static void child_expects_SEND_CERT(void) { execl(SUB, ARG0, "SEND_CERT", NULL); }
NEW_TEST(proto_pdu_SEND_CERT)
{
	pid_t pid;
	struct test_proto_conn server;

	X509_REQ *req;
	EVP_PKEY *key;
	X509 *cert;
	struct cert_subject subj = {
		.fqdn     = "test1.example.net",
		.country  = "US",
		.loc      = "Peoria",
		.state    = "IL",
		.org      = "NiftyLogic",
		.type     = "Agent",
		.org_unit = "R&D"
	};

	test("proto: SEND_CERT PDU");

	key = cert_generate_key(2048);
	assert_not_null("Have a valid key", key);

	req = cert_generate_request(key, &subj);
	assert_not_null("Have a valid CSR", req);

	cert = cert_sign_request(req, NULL, key, 22);
	assert_not_null("Have a signed cert", cert);

	pid = fork_test(&server, child_expects_SEND_CERT);
	assert_int_ne("pdu_send_SEND_CERT returns successfully",
		pdu_send_SEND_CERT(&server.session, cert), 0);
	assert_int_eq("pdu_send_SEND_CERT (no cert) returns 0 bytes written",
		pdu_send_SEND_CERT(&server.session, NULL), 0);
	waitfork(pid, &server);

	protocol_session_deinit(&server.session);

}

static void child_expects_REPORT(void) { execl(SUB, ARG0, "REPORT", NULL); }
NEW_TEST(proto_pdu_REPORT)
{
	pid_t pid;
	struct test_proto_conn server;

	struct job *job;
	struct report *report;

	test("proto: REPORT PDU");
	job = job_new();
	job_start(job);
	job_end(job);

	report = report_new("User", "test1");
	report_action(report, string("Create user"), ACTION_SKIPPED);
	report_action(report, string("Create home dir"), ACTION_SKIPPED);
	job_add_report(job, report);

	pid = fork_test(&server, child_expects_REPORT);
	assert_int_eq("pdu_send_REPORT returns successfully",
		pdu_send_REPORT(&server.session, job), 0);
	waitfork(pid, &server);

	protocol_session_deinit(&server.session);
	job_free(job);
}

NEW_SUITE(proto)
{
	cert_init();
	protocol_ssl_init();

	RUN_TEST(proto_op_names);
	RUN_TEST(proto_pdu_BYE);
	RUN_TEST(proto_pdu_ERROR);
	RUN_TEST(proto_pdu_FACTS);
	RUN_TEST(proto_pdu_POLICY);
	RUN_TEST(proto_pdu_FILE);
	RUN_TEST(proto_pdu_DATA);
	RUN_TEST(proto_pdu_GET_CERT);
	RUN_TEST(proto_pdu_SEND_CERT);
	RUN_TEST(proto_pdu_REPORT);

	cert_deinit();
}
