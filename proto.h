#ifndef _PROTO_H
#define _PROTO_H

/***

  The following namespaces are reserved by this module:
    protocol_*
    pdu_*

 ***/

#include <stdint.h>
#include <sys/types.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "hash.h"
#include "policy.h"

typedef enum {
	PROTOCOL_OP_ERROR = 1,
	PROTOCOL_OP_ACK,
	PROTOCOL_OP_BYE,
	PROTOCOL_OP_GET_POLICY,
	PROTOCOL_OP_SEND_POLICY,
	PROTOCOL_OP_PUT_REPORT,
	PROTOCOL_OP_SEND_REPORT,
} protocol_op;

/**
  protocol_data_unit

  Represents an individual data unit, which corresponds to
  a request or response sent in either direction.
 */
typedef struct {
	protocol_op  op;    /* Operation, one of PROTOCOL_OP_* */
	uint16_t     len;   /* Length of data, in octets */
	uint8_t     *data;  /* Pointer to len octets of data. */

} protocol_data_unit;

/**
  protocol_session

  Encapsulates state data needed to carry out a single session,
  between client and server.  This data type is used for both
  sides of a connection.
 */
typedef struct {
	SSL                *io;        /* IO stream to read from / write to */

	struct manifest    *manifest;  /* Parsed manifest (all policies and host defs) */
	char                fqdn[256]; /* Client's fully qualified domain name */

	protocol_data_unit  send_pdu;  /* PDU to send to the other side */
	protocol_data_unit  recv_pdu;  /* PDU received from other side */

	uint16_t            errnum;    /* Last error received */
	unsigned char      *errstr;    /* Copy of the last error received */
} protocol_session;

/**********************************************************/

int protocol_session_init(protocol_session *session, SSL *io, const char *client_fqdn);
int protocol_session_deinit(protocol_session *session);

void protocol_ssl_init(void);
void protocol_ssl_seed_prng(void);

long protocol_ssl_verify_peer(SSL *ssl, const char *hostname);

SSL_CTX* protocol_ssl_default_context(const char *ca_cert_file,
                                      const char *cert_file,
                                      const char *key_file);

int server_dispatch(protocol_session *session);

struct policy* client_get_policy(protocol_session *session, const struct hash *facts);
int client_disconnect(protocol_session *session);

void protocol_ssl_backtrace(void);

/**********************************************************/

int pdu_read(SSL *io, protocol_data_unit *pdu);
int pdu_write(SSL *io, protocol_data_unit *pdu);
int pdu_receive(protocol_session *session);

int pdu_send_ERROR(protocol_session *pdu, uint16_t err_code, const uint8_t *str, size_t len);
int pdu_encode_ERROR(protocol_data_unit *pdu, uint16_t err_code, const uint8_t *str, size_t len);
int pdu_decode_ERROR(protocol_data_unit *pdu, uint16_t *err_code, uint8_t **str, size_t *len);

int pdu_send_ACK(protocol_session *session);
int pdu_encode_ACK(protocol_data_unit *pdu);
int pdu_decode_ACK(protocol_data_unit *pdu);

int pdu_send_BYE(protocol_session *session);
int pdu_encode_BYE(protocol_data_unit *pdu);
int pdu_decode_BYE(protocol_data_unit *pdu);

int pdu_send_GET_POLICY(protocol_session *session, const struct hash *facts);
int pdu_encode_GET_POLICY(protocol_data_unit *pdu, const struct hash *facts);
int pdu_decode_GET_POLICY(protocol_data_unit *pdu, struct hash *facts);

int pdu_send_SEND_POLICY(protocol_session *session, const struct policy *policy);
int pdu_encode_SEND_POLICY(protocol_data_unit *pdu, const struct policy *policy);
int pdu_decode_SEND_POLICY(protocol_data_unit *pdu, struct policy **policy);

#endif
