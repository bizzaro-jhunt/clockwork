#include "test.h"
#include "assertions.h"

#include "../proto.h"
#include "../cert.h"

#include <sys/types.h>
#include <sys/wait.h>

#define assert_op_name(op) assert_str_eq("Op name for " #op, protocol_op_name(PROTOCOL_OP_ ## op), #op)

struct test_proto_conn {
	pid_t             pid;
	SSL_CTX          *ctx;
	SSL              *ssl;
	protocol_session  session;
};

static int test_proto_conn_client(struct test_proto_conn *client, int r, int w)
{
	BIO *rbio = NULL, *wbio = NULL;

	client->ctx = SSL_CTX_new(TLSv1_method());
	assert_not_null("SSL context is not null (client)", client->ctx);
	if (!client->ctx) { return -1; }
	if (SSL_CTX_set_cipher_list(client->ctx, "ADH") != 1) { return -2; }

	client->ssl = SSL_new(client->ctx);
	assert_not_null("SSL handle is not null (client)", client->ssl);
	if (!client->ssl) { return -3; }

	rbio = BIO_new_fd(r, BIO_CLOSE);
	wbio = BIO_new_fd(w, BIO_CLOSE);

	assert_not_null("Read BIO is not null (client)", rbio);
	assert_not_null("Write BIO is not null (client)", rbio);

	SSL_set_bio(client->ssl, rbio, wbio);
	return 0;
}

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


typedef int (*child_fn)(protocol_session *session);

static pid_t fork_test(struct test_proto_conn *server, child_fn test)
{
	pid_t pid;
	struct test_proto_conn client;
	int rc;
	int s2c[2], c2s[2];

	assert_int_eq("s2c pipe created successfully", pipe(s2c), 0);
	assert_int_eq("c2s pipe created successfully", pipe(c2s), 0);
	assert_int_gte("forked successfully", pid = fork(), 0);

	if (pid == 0) {
		close(s2c[1]);
		close(c2s[0]);

		assert_int_eq("Client test connection setup properly",
			test_proto_conn_client(&client, s2c[0], c2s[1]), 0);

		/*
		assert_not_null("client-CTX is not NULL", client.ctx);
		assert_not_null("client-SSL is not NULL", client.ssl);

		read  = BIO_new_fd(s2c[0], BIO_CLOSE);
		write = BIO_new_fd(c2s[1], BIO_CLOSE);

		assert_not_null("client-read BIO is not NULL", read);
		assert_not_null("client-write BIO is not NULL", write);

		SSL_set_bio(client.ssl, read, write);
		*/
		SSL_connect(client.ssl);
		protocol_session_init(&client.session, client.ssl);

		rc = (*test)(&client.session);
		protocol_session_deinit(&client.session);
		test_proto_conn_free(&client);

		exit(rc);
	} else {
		close(c2s[1]);
		close(s2c[0]);

		assert_int_eq("Server test connection setup properly",
			test_proto_conn_server(server, c2s[0], s2c[1]), 0);

		/*
		assert_not_null("server-CTX is not NULL", server->ctx);
		assert_not_null("server-SSL is not NULL", server->ssl);

		read  = BIO_new_fd(c2s[0], BIO_CLOSE);
		write = BIO_new_fd(s2c[1], BIO_CLOSE);

		assert_not_null("server-read BIO is not NULL", read);
		assert_not_null("server-write BIO is not NULL", write);

		SSL_set_bio(server->ssl, read, write);
		*/
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

void test_proto_op_names()
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

static int child_expects_BYE(protocol_session *s) {
	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_BYE) { return 1; }

	return 0;
}

void test_proto_pdu_BYE()
{
	pid_t pid;
	struct test_proto_conn server;

	test("proto: BYE PDU");
	pid = fork_test(&server, child_expects_BYE);
	assert_int_eq("pdu_send_BYE returns successfully", pdu_send_BYE(&server.session), 0);
	waitfork(pid, &server);

	protocol_session_deinit(&server.session);
}

static int child_expects_ERROR(protocol_session *s)
{
	uint16_t errcode;
	uint8_t *errstr;
	size_t len;

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_ERROR) { return 1; }
	if (pdu_decode_ERROR(RECV_PDU(s), &errcode, &errstr, &len) != 0) { return 2; }
	if (errcode != 404) { return 3; }
	if (strcmp((char*)errstr, "not found") != 0) { return 4; };
	if (len != strlen("not found") + 1) { return 5; }

	free(errstr);
	return 0;
}

void test_proto_pdu_ERROR()
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

static int child_expects_FACTS(protocol_session *s)
{
	struct hash *facts;
	char *v;

	facts = hash_new();

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_FACTS) { return 1; }
	if (pdu_decode_FACTS(RECV_PDU(s), facts) != 0) { return 2; }

	v = hash_get(facts, "fact1");
	if (strcmp(v, "value1") != 0) { return 3; }

	v = hash_get(facts, "fact2");
	if (strcmp(v, "v2") != 0) { return 4; }

	v = hash_get(facts, "nokey");
	if (v) { return 5; }

	hash_free(facts);
	return 0;
}

void test_proto_pdu_FACTS()
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

static int child_expects_POLICY(protocol_session *s)
{
	struct policy *pol;

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_POLICY) { return 1; }
	if (pdu_decode_POLICY(RECV_PDU(s), &pol) != 0) { return 2; }

	if (strcmp(pol->name, "testpolicy") != 0) { return 3; }

	return 0;
}

void test_proto_pdu_POLICY()
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
static int child_expects_FILE(protocol_session *s)
{
	char sha1[SHA1_HEX_DIGEST_SIZE + 1] = {0};

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_FILE) { return 1; }

	memcpy(sha1, RECV_PDU(s)->data, SHA1_HEX_DIGEST_SIZE);
	if (strcmp(PDU_SHA1, sha1) != 0) { return 2; }

	return 0;
}

void test_proto_pdu_FILE()
{
	sha1 checksum;
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

static int child_expects_DATA(protocol_session *s)
{
	char buf[256] = {};

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_DATA) { return 1; }
	if (RECV_PDU(s)->len != 4) { return 50 + RECV_PDU(s)->len; }
	memcpy(buf, RECV_PDU(s)->data, RECV_PDU(s)->len);
	if (strcmp(buf, "xyzy") != 0) { return 3; }

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_DATA) { return 4; }
	if (RECV_PDU(s)->len != 0) { return 5; }

	return 0;
}
void test_proto_pdu_DATA()
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

static int child_expects_GET_CERT(protocol_session *s)
{
	X509_REQ *req = NULL;

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_GET_CERT) { return 1; }
	if (pdu_decode_GET_CERT(RECV_PDU(s), &req) != 0) { return 2; }
	if (!req) { return 3; }

	return 0;
}
void test_proto_pdu_GET_CERT()
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

static int child_expects_SEND_CERT(protocol_session *s)
{
	X509 *cert1 = NULL;
	X509 *cert2 = NULL;

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_SEND_CERT) { return 1; }
	if (pdu_decode_SEND_CERT(RECV_PDU(s), &cert1) != 0) { return 2; }
	if (!cert1) { return 3; }

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_SEND_CERT) { return 4; }
	if (pdu_decode_SEND_CERT(RECV_PDU(s), &cert2) != 0) { return 5; }
	if (cert2) { return 6; }

	return 0;
}
void test_proto_pdu_SEND_CERT()
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

static int child_expects_REPORT(protocol_session *s)
{
	struct job *job = NULL;

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_REPORT) { return 1; }
	if (pdu_decode_REPORT(RECV_PDU(s), &job) != 0) { return 2; }
	if (!job) { return 3; }

	job_free(job);
	return 0;
}
void test_proto_pdu_REPORT()
{
	pid_t pid;
	struct test_proto_conn server;

	struct job *job;
	struct report *report;

	test("proto: REPORT PDU");
	job = job_new();
	job_start(job);
	job_end(job);

	report = report_new(string("User"), string("test1"));
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

void test_suite_proto()
{
	cert_init();
	protocol_ssl_init();

	test_proto_op_names();
	test_proto_pdu_BYE();
	test_proto_pdu_ERROR();
	test_proto_pdu_FACTS();
	test_proto_pdu_POLICY();
	test_proto_pdu_FILE();
	test_proto_pdu_DATA();
	test_proto_pdu_GET_CERT();
	test_proto_pdu_SEND_CERT();
	test_proto_pdu_REPORT();

	cert_deinit();
}
