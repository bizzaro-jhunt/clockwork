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
	PROTOCOL_OP_ACK,
	PROTOCOL_OP_BYE,
	PROTOCOL_OP_GET_POLICY,
	PROTOCOL_OP_SEND_POLICY,
	PROTOCOL_OP_GET_FILE,
	PROTOCOL_OP_FILE_DATA,
	PROTOCOL_OP_PUT_REPORT,
	PROTOCOL_OP_SEND_REPORT,
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
  Set up the default OpenSSL context.

  This function wraps up all of the required SSL options for
  both client and server connections.

  @param  ca_cert_file    Path to the Certificate Authorities certificate.
  @param  cert_file       Path to this hosts certificate.
  @param  key_file        Path to this hosts private key.

  @returns a pointer to a valid SSL_CTX object that can be used
           to set up new SSL connections.
 */
SSL_CTX* protocol_ssl_default_context(const char *ca_cert_file,
                                      const char *cert_file,
                                      const char *key_file);

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
  Send an ERROR PDU to the remote party.

  @param  session     The current session, containing the IO stream
                      and the PDU buffer.
  @param  err_code    The error code.
  @param  str         A descriptive error string.
  @param  len         Length of the descriptive error string.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_ERROR(protocol_session *session, uint16_t err_code, const uint8_t *str, size_t len);

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
  Send an ACK PDU to the remote party.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_ACK(protocol_session *session);

/**
  Send a BYE PDU to the remote party.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_BYE(protocol_session *session);

/**
  Send a GET_POLICY PDU to the server.

  This type of PDU is sent from the client to the server, sending along
  a hash of the local facts in order to receive a generated policy.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.
  @param  facts      A hash of local client facts to send.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_GET_POLICY(protocol_session *session, const struct hash *facts);

/**
  Decode a GET_POLICY PDU sent from a client, storing the facts in \a facts.

  @param  pdu      PDU to decode.
  @param  facts    Pointer to a hash to store client facts in.

  @returns 0 on success, non-zero on failure.
 */
int pdu_decode_GET_POLICY(protocol_data_unit *pdu, struct hash *facts);

/**
  Send a SEND_POLICY PDU to a client.

  This type of PDU is sent from the server to a client, and includes the
  policy generated from the client facts given in the GET_POLICY PDU.

  @param  session    The current session.  Contains the IO stream,
                     and the PDU buffer.
  @param  policy     The generated policy to send.

  @returns 0 on success, non-zero on failure.
 */
int pdu_send_SEND_POLICY(protocol_session *session, const struct policy *policy);

/**
  Decode a SEND_POLICY PDU sent from the server, storing the policy in \a policy.

  @param  pdu       PDU to decode.
  @param  policy    A pointer to a pointer to a policy, where the policy
                    will be stored.

  @returns 0 on success, non-zero on failure.
 */
int pdu_decode_SEND_POLICY(protocol_data_unit *pdu, struct policy **policy);

int pdu_send_GET_FILE(protocol_session *session, sha1 *checksum);
int pdu_decode_GET_FILE(protocol_data_unit *pdu, sha1 *checksum);

int pdu_send_FILE_DATA(protocol_session *session, int srcfd);
int pdu_decode_FILE_DATA(protocol_data_unit *pdu, int dstfd);

#endif
