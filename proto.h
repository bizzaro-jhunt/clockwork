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

#ifndef _PROTO_H
#define _PROTO_H

#include <stdint.h>
#include <sys/types.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "hash.h"
#include "policy.h"

/** @file proto.h

  This module provides protocol helpers for the client and server
  binaries, and abstracts the underlying mechanisms of the protocol
  exchanged between client and server.  This includes the structure
  of the PDUs on which the protocol is built, as well as the encoding,
  transmission and decoding of said PDUs.

  The following namespaces are reserved by this module:

  - protocol_*
  - pdu_*

 */

/**
  Protocol Operations

  These values represent the different types of protocol data units
  that can be sent between client and server.
 */
typedef enum {
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
} protocol_op;

/**
  Represents an individual data unit, which corresponds to
  a request or response sent in either direction.
 */
typedef struct {
	/** The type of protocol data unit. */
	protocol_op op;

	/** Length of the PDU, in octets. */
	uint16_t len;

	/** The payload of the PDU, a pointer to \a len octets of data. */
	uint8_t *data;

} protocol_data_unit;

/**
  Encapsulates state data needed to carry out a single session,
  between client and server.  This data type is used for both
  sides of a connection.
 */
typedef struct {
	/** OpenSSL IO stream to read from / write to */
	SSL *io;

	/** PDU to send to the remote party. */
	protocol_data_unit send_pdu;
	/** PDU received from remote party. */
	protocol_data_unit recv_pdu;

	/** Last error code received. */
	uint16_t errnum;
	/** Copy of the last error received. */
	unsigned char *errstr;
} protocol_session;

/**
  Macro to retrieve a pointer to a session's send PDU buffer.

  @param  session_ptr    Pointer to the current session.
  @returns a pointer to the protocol_data_unit buffer used for
           sending PDUs to the remote side.
 */
#define SEND_PDU(session_ptr) (&(session_ptr)->send_pdu)

/**
  Macro to retrieve a pointer to a session's receive PDU buffer.

  @param  session_ptr    Pointer to the current session.
  @returns a pointer to the protocol_data_unit buffer used for
           receiving PDUs from the remote side.
 */
#define RECV_PDU(session_ptr) (&(session_ptr)->recv_pdu)

/**********************************************************/

/**
  Generate a human-frindly textual name for a protocol operation.

  @param  op    Protocol operator to look up.

  @returns a constant string describint the protocol operator.
 */
const char* protocol_op_name(protocol_op op);

/**
  Initialize a protocol_session structure.

  @note This function does not allocate the protocol_session structure itself.
        Nevertheless, the caller should match this call with a call to
        protocol_session_deinit, to free memory allocated by this function.

  @param  session        Pointer to the protocol_session to initialize.
  @param  io             OpenSSL IO stream for the duplex connection.

  @returns 0 on success, non-zero on failure.
 */
int protocol_session_init(protocol_session *session, SSL *io);

/**
  De-initialize a protocol_session structure.

  @note Since protocol_session_init does not allocate the protocol_session
        structure itself, this function does not free the pointer passed to
        it.  If the caller allocates the structure on the heap, the caller
        is responsible for freeing that memory.

  @param  session    Pointer to the protocol session to initialize.

  @returns 0 on success, non-zero on failure.
 */
int protocol_session_deinit(protocol_session *session);

/**
  Initialize the OpenSSL library.
 */
void protocol_ssl_init(void);

/**
  Callback to verify the peer's certificate by comparing the
  subjectAltName/DNS certificate extension against \a hostname.

  @note How the \a hostname parameter is determined is left to
        the caller.  For true security, some sort of reverse
        DNS lookup against the peer address information is
        strongly suggested.

  @see protocol_reverse_lookup_verify

  @param  ssl         SSL object containing the peer certificate.
  @param  hostname    Peer hostname to verify.

  @returns 0 on success, or an OpenSSL error code on failure.
 */
long protocol_ssl_verify_peer(SSL *ssl, const char *hostname);

/**
  Log all SSL errors on the stack, using the DEBUG priority.
 */
void protocol_ssl_backtrace(void);

/**
  Perform a Reverse DNS lookup on a connected socket.

  This function is used during the peer certificate verification
  process;  the FQDN of the remote host is looked up so that it
  can be compared to that stored in the certificate extensions
  (subjectAltName / DNS).

  @see protocol_ssl_verify_peer

  @param  sockfd    A connected socket file descriptor.
  @param  buf       Buffer to store the confirmed FQDN.
  @param  len       Length of \a buf.

  @returns 0 on success, non-zero on failure.
 */
int protocol_reverse_lookup_verify(int sockfd, char *buf, size_t len);

/**********************************************************/

/**
  Read a PDU from \a io, storing it in \a pdu.

  @param  io     SSL IO stream to read the PDU from.
  @param  pdu    Pointer to store PDU data in.

  @returns 0 on success, non-zero on failure.
 */
int pdu_read(SSL *io, protocol_data_unit *pdu);

/**
  Write PDU \a pdu to \a io.

  @param  io     SSL IO stream to write the PDU to.
  @param  pdu    The PDU to write.

  @returns 0 on success, non-zero on failure.
 */
int pdu_write(SSL *io, protocol_data_unit *pdu);

/**
  Receive the next PDU from the remote party.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.

  @returns 0 on success, non-zero on failure.
 */
int pdu_receive(protocol_session *session);

/**
  Send a simple (data-less) PDU to the remote party.

  This function is used by macros like pdu_send_BYE, which
  do not need to send additional data to the other side.

  @param  session     The current session, containing the IO stream
                      and the PDU buffer.
  @param  op          The protocol operator (PDU type) to send.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_simple(protocol_session *session, protocol_op op);

/**
  Send an ERROR PDU to the remote party.

  @param  session     The current session, containing the IO stream
                      and the PDU buffer.
  @param  err_code    The error code.
  @param  str         A descriptive error string.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_ERROR(protocol_session *session, uint16_t err_code, const char *str);

/**
  Decode an ERROR PDU, storing the contents in \a err_code, \a str and \a len.

  @param  pdu         PDU to decode.
  @param  err_code    Pointer to an unsigned integer to store the error code in.
  @param  str         Pointer to a string buffer to store the error message in.
  @param  len         Pointer to a size_t to store the length of \a str in.

  @returns 0 on success, non-zero on failure.
 */
int pdu_decode_ERROR(protocol_data_unit *pdu, uint16_t *err_code, uint8_t **str, size_t *len);

/**
  Send a HELLO PDU to the report part.

  @param  s    The current session.  Contains the IO stream and the PDU buffer.

  @returns 0 on success, non-zero on failure.
 */
#define pdu_send_HELLO(s) pdu_send_simple((s), PROTOCOL_OP_HELLO)

/**
  Send a BYE PDU to the remote party.

  @param  s    The current session.  Contains the IO stream and the PDU buffer.

  @returns 0 on success, non-zero on failure.
 */
#define pdu_send_BYE(s) pdu_send_simple((s), PROTOCOL_OP_BYE)

/**
  Send a FACTS PDU to the server.

  This type of PDU is sent from the client to the server, sending along
  a hash of the local facts in order to receive a generated policy.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.
  @param  facts      A hash of local client facts to send.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_FACTS(protocol_session *session, const struct hash *facts);

/**
  Decode a GET_POLICY PDU sent from a client, storing the facts in \a facts.

  @param  pdu      PDU to decode.
  @param  facts    Pointer to a hash to store client facts in.

  @returns 0 on success, non-zero on failure.
 */
int pdu_decode_FACTS(protocol_data_unit *pdu, struct hash *facts);

/**
  Send a POLICY PDU to a client.

  This type of PDU is sent from the server to a client, and includes the
  policy generated from the client facts given in the FACTS PDU.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.
  @param  policy     The generated policy to send.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_POLICY(protocol_session *session, const struct policy *policy);

/**
  Decode a POLICY PDU sent from the server, storing the policy in \a policy.

  @param  pdu       PDU to decode.
  @param  policy    A pointer to a pointer to a policy, where the policy
                    will be stored.

  @returns 0 on success, non-zero on failure.
 */
int pdu_decode_POLICY(protocol_data_unit *pdu, struct policy **policy);

/**
  Send a FILE PDU to the server.

  FILE PDUs are sent by clients to server in order to request the contents
  of a res_file that need to be refreshed on the client machine.  For security
  reasons, only the checksum is sent, to prevent "leeching" of files.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.
  @param  checksum   SHA1 checksum of the file (obtained from the policy).

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_FILE(protocol_session *session, sha1 *checksum);

/**
  Send a DATA PDU to a client.

  DATA PDUs are sent by servers to clients, in response to a FILE PDU.
  Each DATA PDU represents a chunk of the file, sent in order.  The final
  (EOF) DATA PDU is sent with a zero length.

  Each call to pdu_send_DATA will read more data out of \a srcfd until an
  EOF occurs.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.
  @param  srcfd      File descriptor of the file to transfer.

  @returns the number of bytes read from \a srcfd on success,
           or -1 on failure.  When an EOF occurs on \a srcfd,
           0 is returned and the EOF DATA PDU is sent.
 */
int pdu_send_DATA(protocol_session *session, int srcfd, const char *data);

/**
  Send a GET_CERT PDU to the server.

  GET_CERT PDUs are used by clients to request a signed certificate from
  a certificate signing request.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.
  @param  csr        The client's certificate signing request.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_GET_CERT(protocol_session *session, X509_REQ  *csr);

/**
  Decode a GET_CERT PDU sent by a client.

  @param  pdu      PDU to decode.
  @param  csr      Pointer to a pointer to a CSR structure,
                   where the request will be stored.

  @returns 0 on success, non-zero on failure.
 */
int pdu_decode_GET_CERT(protocol_data_unit *pdu, X509_REQ **csr);

/**
  Send a SEND_CERT PDU to a client.

  SEND_CERT PDUs are sent in response to a GET_CERT, if the policy
  master has a signed certificate on file for the client.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.
  @param  cert       The signed certificate for the client.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_SEND_CERT(protocol_session *session, X509  *cert);

/**
  Decode a SEND_CERT PDU sent by the server.

  @param  pdu      PDU to decode.
  @param  cert     Pointer to a pointer to a certificate structure,
                   where the certificate will be stored.

  @returns 0 on success, non-zero on failure.
 */
int pdu_decode_SEND_CERT(protocol_data_unit *pdu, X509 **cert);

/**
  Send a REPORT PDU to the policy master.

  REPORT PDUs are sent to the policy master by clients after they
  have evaluated and enforced their policy locally.  The PDU contains
  a packed job structure that represents the run and everything that
  was attempted to bring the local system into compliance.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.
  @param  job        The job representing the report to send.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_REPORT(protocol_session *session, struct job *job);

/**
  Decode a REPORT PDU sent by the client.

  @param  pdu      PDU to decode.
  @param  job      Pointer to a pointer to a job structure,
                   where the report will be stored.

  @returns 0 on success, non-zero on failure.
 */
int pdu_decode_REPORT(protocol_data_unit *pdu, struct job **job);

#endif
