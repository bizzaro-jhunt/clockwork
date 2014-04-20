/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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

#include "../src/proto.h"
#include "../src/cert.h"

#include <sys/types.h>
#include <sys/wait.h>

#define SUB  "t/helpers/proto"
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

	isnt_null(server->ctx = SSL_CTX_new(TLSv1_method()), "SSL context (server)");

	/* this failed for REPORT; maybe we hit our limit? */
	if (!server->ctx) { protocol_ssl_backtrace(); }
	if (!server->ctx) { return -1; }
	if (SSL_CTX_set_cipher_list(server->ctx, "ADH") != 1) { return -2; }

	dh = DH_generate_parameters(16, 2, NULL, NULL);
	if (!DH_check(dh, &ignored)) { DH_free(dh); return -3; }
	if (!DH_generate_key(dh))    { DH_free(dh); return -4; }

	SSL_CTX_set_tmp_dh(server->ctx, dh);
	DH_free(dh);

	isnt_null(server->ssl = SSL_new(server->ctx), "SSL handle (server)");
	if (!server->ssl) { return -3; }

	isnt_null(rbio = BIO_new_fd(r, BIO_CLOSE), "Read BIO handle");
	isnt_null(wbio = BIO_new_fd(w, BIO_CLOSE), "Write BIO handle");

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
	int s2c[2], c2s[2];

	ok(pipe(s2c) == 0, "s2c pipe created successfully");
	ok(pipe(c2s) == 0, "c2s pipe created successfully");

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
		ok(pid > 0, "forked");
		close(c2s[1]);
		close(s2c[0]);

		ok(test_proto_conn_server(server, c2s[0], s2c[1]) == 0,
			"server test connection setup");

		SSL_accept(server->ssl);
		protocol_session_init(&server->session, server->ssl);

		return pid;
	}
}

static void waitfork(pid_t pid, struct test_proto_conn *conn)
{
	int status;
	waitpid(pid, &status, 0);
	ok(WIFEXITED(status), "child exited normally");
	is_int(WEXITSTATUS(status), 0, "child exit status");
	test_proto_conn_free(conn);
}
/***********************************************************************/

static void child_expects_BYE(void) { execl(SUB, ARG0, "BYE", NULL); }
static void child_expects_ERROR(void) { execl(SUB, ARG0, "ERROR", NULL); }
static void child_expects_FACTS(void) { execl(SUB, ARG0, "FACTS", NULL); }
static void child_expects_POLICY(void) { execl(SUB, ARG0, "POLICY", NULL); }
#define PDU_SHA1 "78745d280fea5e30dbaf54e1824a0ecc89e4b505"
static void child_expects_FILE(void) { execl(SUB, ARG0, "FILE", PDU_SHA1, NULL); }
static void child_expects_DATA(void) { execl(SUB, ARG0, "DATA", NULL); }
static void child_expects_GET_CERT(void) { execl(SUB, ARG0, "GET_CERT", NULL); }
static void child_expects_SEND_CERT(void) { execl(SUB, ARG0, "SEND_CERT", NULL); }
static void child_expects_REPORT(void) { execl(SUB, ARG0, "REPORT", NULL); }

TESTS {
	cert_init();
	protocol_ssl_init();

	subtest {
		is_string(protocol_op_name(PROTOCOL_OP_ERROR),     "ERROR",     "ERROR op name");
		is_string(protocol_op_name(PROTOCOL_OP_HELLO),     "HELLO",     "HELLO op name");
		is_string(protocol_op_name(PROTOCOL_OP_FACTS),     "FACTS",     "FACTS op name");
		is_string(protocol_op_name(PROTOCOL_OP_POLICY),    "POLICY",    "POLICY op name");
		is_string(protocol_op_name(PROTOCOL_OP_FILE),      "FILE",      "FILE op name");
		is_string(protocol_op_name(PROTOCOL_OP_DATA),      "DATA",      "DATA op name");
		is_string(protocol_op_name(PROTOCOL_OP_REPORT),    "REPORT",    "REPORT op name");
		is_string(protocol_op_name(PROTOCOL_OP_BYE),       "BYE",       "BYE op name");
		is_string(protocol_op_name(PROTOCOL_OP_GET_CERT),  "GET_CERT",  "GET_CERT op name");
		is_string(protocol_op_name(PROTOCOL_OP_SEND_CERT), "SEND_CERT", "SEND_CERT op name");

		is_null(protocol_op_name(999999), "unknown op name");
	}

	subtest {
		pid_t pid;
		struct test_proto_conn server;

		pid = fork_test(&server, child_expects_BYE);
		ok(pdu_send_BYE(&server.session) == 0, "pdu_send_BYE returns 0");
		waitfork(pid, &server);

		protocol_session_deinit(&server.session);
	}

	subtest {
		pid_t pid;
		struct test_proto_conn server;

		pid = fork_test(&server, child_expects_ERROR);
		ok(pdu_send_ERROR(&server.session, 404, "not found") == 0,
			"pdu_send_ERROR returns 0");
		waitfork(pid, &server);

		protocol_session_deinit(&server.session);
	}

	subtest {
		struct hash *facts;
		pid_t pid;
		struct test_proto_conn server;

		facts = hash_new();
		hash_set(facts, "fact1", "value1");
		hash_set(facts, "fact2", "v2");

		pid = fork_test(&server, child_expects_FACTS);
		ok(pdu_send_FACTS(&server.session, facts) == 0,
			"pdu_send_FACTS returns successfully");
		waitfork(pid, &server);

		protocol_session_deinit(&server.session);
		hash_free(facts);
	}

	subtest {
		struct policy *pol;
		pid_t pid;
		struct test_proto_conn server;

		pol = policy_new("testpolicy");
		pid = fork_test(&server, child_expects_POLICY);
		ok(pdu_send_POLICY(&server.session, pol) == 0,
			"pdu_send_POLICY return successfully");
		waitfork(pid, &server);

		protocol_session_deinit(&server.session);
		policy_free(pol);
	}

	subtest {
		struct SHA1 checksum;
		pid_t pid;
		struct test_proto_conn server;

		sha1_init(&checksum, PDU_SHA1);
		pid = fork_test(&server, child_expects_FILE);
		ok(pdu_send_FILE(&server.session, &checksum) == 0,
			"pdu_send_FILE return successfully");
		waitfork(pid, &server);

		protocol_session_deinit(&server.session);
	}

	subtest {
		int fd[2];
		pid_t pid;
		struct test_proto_conn server;

		ok(pipe(fd) == 0, "pipe created");
		is_int(write(fd[1], "xyzy", 4), 4, "write 4 bytes to pipe");
		close(fd[1]);

		pid = fork_test(&server, child_expects_DATA);
		ok(pdu_send_DATA(&server.session, fd[0], NULL) != 0, "first DATA pdu");
		ok(pdu_send_DATA(&server.session, fd[0], NULL) == 0, "second DATA pdu");
		close(fd[0]);
		waitfork(pid, &server);

		protocol_session_deinit(&server.session);
	}

	subtest {
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

		isnt_null(key = cert_generate_key(2048), "generated key");
		isnt_null(req = cert_generate_request(key, &subj), "generated CSR");

		pid = fork_test(&server, child_expects_GET_CERT);
		ok(pdu_send_GET_CERT(&server.session, req) == 0,
			"pdu_send_GET_CERT returns successfully");
		waitfork(pid, &server);

		protocol_session_deinit(&server.session);
	}

	subtest {
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

		isnt_null(key = cert_generate_key(2048), "generated key");
		isnt_null(req = cert_generate_request(key, &subj), "generated CSR");
		isnt_null(cert = cert_sign_request(req, NULL, key, 22), "signed cert");

		pid = fork_test(&server, child_expects_SEND_CERT);
		ok(pdu_send_SEND_CERT(&server.session, cert) > 0,
			"pdu_send_SEND_CERT returns successfully");
		ok(pdu_send_SEND_CERT(&server.session, NULL) == 0,
			"pdu_send_SEND_CERT (no cert) returns 0 bytes written");
		waitfork(pid, &server);

		protocol_session_deinit(&server.session);
	}

	subtest {
		pid_t pid;
		struct test_proto_conn server;

		struct job *job;
		struct report *report;

		job = job_new();
		job_start(job);
		job_end(job);

		report = report_new("User", "test1");
		report_action(report, string("Create user"), ACTION_SKIPPED);
		report_action(report, string("Create home dir"), ACTION_SKIPPED);
		job_add_report(job, report);

		pid = fork_test(&server, child_expects_REPORT);
		ok(pdu_send_REPORT(&server.session, job) == 0,
			"pdu_send_REPORT returns successfully");
		waitfork(pid, &server);

		protocol_session_deinit(&server.session);
		job_free(job);
	}

	cert_deinit();
	done_testing();
}
