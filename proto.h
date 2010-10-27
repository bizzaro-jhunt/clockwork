#ifndef _PROTO_H
#define _PROTO_H

#include <stdint.h>
#include <sys/types.h>
#include <openssl/ssl.h>

typedef enum {
	PROTOCOL_OP_NOOP = 1,
	PROTOCOL_OP_ERROR,
	PROTOCOL_OP_ACK,
	PROTOCOL_OP_BYE,
	PROTOCOL_OP_GET_VERSION,
	PROTOCOL_OP_SEND_VERSION,
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

	protocol_data_unit  send_pdu;  /* PDU to send to the other side */
	protocol_data_unit  recv_pdu;  /* PDU received from other side */

	uint16_t            errnum;    /* Last error received */
	unsigned char      *errstr;    /* Copy of the last error received */
} protocol_session;

int pdu_send_NOOP(protocol_session *session);
int pdu_encode_NOOP(protocol_data_unit *pdu);
int pdu_decode_NOOP(protocol_data_unit *pdu);

int pdu_send_ERROR(protocol_session *pdu, uint16_t err_code, const uint8_t *str, size_t len);
int pdu_encode_ERROR(protocol_data_unit *pdu, uint16_t err_code, const uint8_t *str, size_t len);
int pdu_decode_ERROR(protocol_data_unit *pdu, uint16_t *err_code, uint8_t **str, size_t *len);

int pdu_send_ACK(protocol_session *session);
int pdu_encode_ACK(protocol_data_unit *pdu);
int pdu_decode_ACK(protocol_data_unit *pdu);

int pdu_send_BYE(protocol_session *session);
int pdu_encode_BYE(protocol_data_unit *pdu);
int pdu_decode_BYE(protocol_data_unit *pdu);

int pdu_send_GET_VERSION(protocol_session *session);
int pdu_encode_GET_VERSION(protocol_data_unit *pdu);
int pdu_decode_GET_VERSION(protocol_data_unit *pdu);

int pdu_send_SEND_VERSION(protocol_session *session, uint32_t version);
int pdu_encode_SEND_VERSION(protocol_data_unit *pdu, uint32_t version);
int pdu_decode_SEND_VERSION(protocol_data_unit *pdu, uint32_t *version);

int pdu_send_GET_POLICY(protocol_session *session);
int pdu_encode_GET_POLICY(protocol_data_unit *pdu);
int pdu_decode_GET_POLICY(protocol_data_unit *pdu);

int pdu_send_SEND_POLICY(protocol_session *session, const uint8_t *policy, size_t len);
int pdu_encode_SEND_POLICY(protocol_data_unit *pdu, const uint8_t *policy, size_t len);
int pdu_decode_SEND_POLICY(protocol_data_unit *pdu, uint8_t **policy, size_t *len);

int pdu_read(SSL *io, protocol_data_unit *pdu);
int pdu_write(SSL *io, protocol_data_unit *pdu);
int pdu_receive(protocol_session *session);

void init_openssl(void);

int protocol_session_init(protocol_session *session, SSL *io);
int protocol_session_deinit(protocol_session *session);

int server_dispatch(protocol_session *session);

uint32_t client_get_policy_version(protocol_session *session);
int client_disconnect(protocol_session *session);

#endif
