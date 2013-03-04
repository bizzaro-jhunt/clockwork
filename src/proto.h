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

#ifndef PROTO_H
#define PROTO_H

#include <stdint.h>
#include <sys/types.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "policy.h"

/**
  Protocol Operations

  These values represent the different types of protocol data units
  that can be sent between client and server.
 */
enum proto_op {
	PROTOCOL_OP_ERROR = 1,
	PROTOCOL_OP_HELLO,
	PROTOCOL_OP_FACTS,
	PROTOCOL_OP_POLICY,
	PROTOCOL_OP_FILE,
	PROTOCOL_OP_DATA,
	PROTOCOL_OP_REPORT,
	PROTOCOL_OP_BYE,

	PROTOCOL_OP_GET_CERT,
	PROTOCOL_OP_SEND_CERT
};

/**
  Protocol Data Unit

  Represents an individual data unit, which corresponds to
  a request or response sent in either direction.
 */
struct pdu {
	enum proto_op  op;    /* type of PDU */
	uint16_t       len;   /* length of PDU paylod */
	uint8_t       *data;  /* PDU payload */
};

/**
  Protocol Session

  Encapsulates state data needed to carry out a single session,
  between client and server.  This data type is used for both
  sides of a connection.
 */
struct session {
	SSL *io;  /* OpenSSL IO stream to read from / write to */

	struct pdu     send_pdu; /* PDU to send to remote end */
	struct pdu     recv_pdu; /* PDU recived from remote end */

	uint16_t       errnum;   /* last error code recieved */
	unsigned char *errstr;   /* last error message received */
};

#define SEND_PDU(session_ptr) (&(session_ptr)->send_pdu)
#define RECV_PDU(session_ptr) (&(session_ptr)->recv_pdu)

/**********************************************************/

const char* protocol_op_name(enum proto_op op);

int protocol_session_init(struct session *s, SSL *io);
int protocol_session_deinit(struct session *s);

void protocol_ssl_init(void);

long protocol_ssl_verify_peer(SSL *ssl, const char *hostname);
int protocol_reverse_lookup_verify(int sockfd, char *buf, size_t len);

void protocol_ssl_backtrace(void);


/**********************************************************/

int pdu_read(SSL *io, struct pdu *pdu);
int pdu_write(SSL *io, struct pdu *pdu);

int pdu_receive(struct session *s);
int pdu_send_simple(struct session *s, enum proto_op op);

/**
  Send a HELLO PDU to the remote end.

  On success, returns zero.  On failure, returns non-zero.
 */
#define pdu_send_HELLO(s) pdu_send_simple((s), PROTOCOL_OP_HELLO)

/**
  Send a BYE PDU to the remote end.

  On success, returns zero.  On failure, returns non-zero.
 */
#define pdu_send_BYE(s) pdu_send_simple((s), PROTOCOL_OP_BYE)

int pdu_send_ERROR(struct session *s, uint16_t err_code, const char *str);
int pdu_send_FACTS(struct session *s, const struct hash *facts);
int pdu_send_POLICY(struct session *s, const struct policy *policy);
int pdu_send_FILE(struct session *s, struct SHA1 *checksum);
int pdu_send_DATA(struct session *s, int srcfd, const char *data);
int pdu_send_GET_CERT(struct session *s, X509_REQ  *csr);
int pdu_send_SEND_CERT(struct session *s, X509  *cert);
int pdu_send_REPORT(struct session *s, struct job *job);

int pdu_decode_ERROR(struct pdu *pdu, uint16_t *err_code, uint8_t **str, size_t *len);
int pdu_decode_FACTS(struct pdu *pdu, struct hash *facts);
int pdu_decode_POLICY(struct pdu *pdu, struct policy **policy);
int pdu_decode_GET_CERT(struct pdu *pdu, X509_REQ **csr);
int pdu_decode_SEND_CERT(struct pdu *pdu, X509 **cert);
int pdu_decode_REPORT(struct pdu *pdu, struct job **job);

#endif
