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

#include "../../src/clockwork.h"
#include "../../src/proto.h"
#include "../../src/cert.h"

/* usage: ./test/sub/proto PDU_TYPE

   where PDU_TYPE can be:
     BYE
     ERROR
     FACTS
     POLICY
     FILE
     DATA
     GET_CERT
     SEND_CERT
 */

struct conn {
	pid_t             pid;
	SSL_CTX          *ctx;
	SSL              *ssl;
	struct session  session;
};

#define CW_EBADCALL 99
#define CW_ENOHNDLR 98
#define CW_ENOTIMPL 97
#define CW_ENOCTX   81
#define CW_ECIPHER  82
#define CW_ENOSSL   83
#define CW_ENOBIO   84

#define CW_EFAIL    10

static int setup(struct conn *c)
{
	cert_init();
	protocol_ssl_init();

	BIO *rbio = NULL, *wbio = NULL;

	c->ctx = SSL_CTX_new(TLSv1_method());
	if (!c->ctx) { return CW_ENOCTX; }
	if (SSL_CTX_set_cipher_list(c->ctx, "ADH") != 1) { return CW_ECIPHER; }

	c->ssl = SSL_new(c->ctx);
	if (!c->ssl) { return CW_ENOSSL; }

	rbio = BIO_new_fd(0, BIO_CLOSE);
	wbio = BIO_new_fd(1, BIO_CLOSE);
	if (!rbio || !wbio) { return CW_ENOBIO; }

	SSL_set_bio(c->ssl, rbio, wbio);
	SSL_connect(c->ssl);
	protocol_session_init(&c->session, c->ssl);
	return 0;
}

static void teardown(struct conn *c)
{
	protocol_session_deinit(&c->session);
//	SSL_free(c->ssl);
//	SSL_CTX_free(c->ctx);
	cert_deinit();
}

#define DISPATCH(x,c) if (strcmp(argv[1], #x) == 0) rc = main_ ## x (argc, argv, (c))
#define HANDLER(x) int main_ ## x (int, char**, struct session*)
#define HANDLE(x) int main_ ## x (int argc, char **argv, struct session *s)

HANDLER(BYE);
HANDLER(ERROR);
HANDLER(FACTS);
HANDLER(POLICY);
HANDLER(FILE);
HANDLER(DATA);
HANDLER(GET_CERT);
HANDLER(SEND_CERT);

int main(int argc, char **argv)
{
	int rc;
	struct conn c;

	if (argc < 2) { return CW_EBADCALL; }

	rc = setup(&c);
	if (rc != 0) { return rc; };
	rc = CW_ENOHNDLR;

	DISPATCH(BYE,       &c.session);
	DISPATCH(ERROR,     &c.session);
	DISPATCH(FACTS,     &c.session);
	DISPATCH(POLICY,    &c.session);
	DISPATCH(FILE,      &c.session);
	DISPATCH(DATA,      &c.session);
	DISPATCH(GET_CERT,  &c.session);
	DISPATCH(SEND_CERT, &c.session);

	teardown(&c);
	return rc;
}


HANDLE(BYE)
{
	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_BYE) { return CW_EFAIL+1; }
	return 0;
}

HANDLE(ERROR)
{
	uint16_t errcode;
	uint8_t *errstr;
	size_t len;

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_ERROR) { return CW_EFAIL+1; }
	if (pdu_decode_ERROR(RECV_PDU(s), &errcode, &errstr, &len) != 0) { return CW_EFAIL+2; }
	if (errcode != 404) { return CW_EFAIL+3; }
	if (strcmp((char*)errstr, "not found") != 0) { return CW_EFAIL+4; };
	if (len != strlen("not found") + 1) { return CW_EFAIL+5; }

	free(errstr);
	return 0;
}

HANDLE(FACTS)
{
	struct hash *facts;
	char *v;

	facts = hash_new();

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_FACTS) { return CW_EFAIL+1; }
	if (pdu_decode_FACTS(RECV_PDU(s), facts) != 0) { return CW_EFAIL+2; }

	v = hash_get(facts, "fact1");
	if (strcmp(v, "value1") != 0) { return CW_EFAIL+3; }

	v = hash_get(facts, "fact2");
	if (strcmp(v, "v2") != 0) { return CW_EFAIL+4; }

	v = hash_get(facts, "nokey");
	if (v) { return CW_EFAIL+5; }

	hash_free(facts);
	return 0;
}

HANDLE(POLICY)
{
	struct policy *pol;

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_POLICY) { return CW_EFAIL+1; }
	if (pdu_decode_POLICY(RECV_PDU(s), &pol) != 0) { return CW_EFAIL+2; }

	if (strcmp(pol->name, "testpolicy") != 0) { return CW_EFAIL+3; }

	return 0;
}

HANDLE(FILE)
{
	char sha1[SHA1_HEXLEN] = {0};

	if (argc != 3) { return CW_EBADCALL; }
	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_FILE) { return CW_EFAIL+1; }

	memcpy(sha1, RECV_PDU(s)->data, SHA1_HEXLEN-1);
	if (strcmp(argv[2], sha1) != 0) { return CW_EFAIL+2; }

	return 0;
}

HANDLE(DATA)
{
	char buf[256] = {};

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_DATA) { return CW_EFAIL+1; }
	if (RECV_PDU(s)->len != 4) { return 50 + RECV_PDU(s)->len; }
	memcpy(buf, RECV_PDU(s)->data, RECV_PDU(s)->len);
	if (strcmp(buf, "xyzy") != 0) { return CW_EFAIL+3; }

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_DATA) { return CW_EFAIL+4; }
	if (RECV_PDU(s)->len != 0) { return CW_EFAIL+5; }

	return 0;
}

HANDLE(GET_CERT)
{
	X509_REQ *req = NULL;

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_GET_CERT) { return CW_EFAIL+1; }
	if (pdu_decode_GET_CERT(RECV_PDU(s), &req) != 0) { return CW_EFAIL+2; }
	if (!req) { return CW_EFAIL+3; }

	return 0;
}

HANDLE(SEND_CERT)
{
	X509 *cert1 = NULL;
	X509 *cert2 = NULL;

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_SEND_CERT) { return CW_EFAIL+1; }
	if (pdu_decode_SEND_CERT(RECV_PDU(s), &cert1) != 0) { return CW_EFAIL+2; }
	if (!cert1) { return CW_EFAIL+3; }

	pdu_receive(s);
	if (RECV_PDU(s)->op != PROTOCOL_OP_SEND_CERT) { return CW_EFAIL+4; }
	if (pdu_decode_SEND_CERT(RECV_PDU(s), &cert2) != 0) { return CW_EFAIL+5; }
	if (cert2) { return CW_EFAIL+6; }

	return 0;
}
