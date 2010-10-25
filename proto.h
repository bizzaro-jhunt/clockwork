#ifndef _PROTO_H
#define _PROTO_H

#include <stdint.h>
#include <sys/types.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include "net.h"

#define PROTO_LINE_MAX 256

typedef enum {
	PROTOCOL_OP_NOOP = 19,
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
	protocol_op  op;    /* Operation, on of PROTOCOL_OP_* */
	uint16_t     len;   /* Length of data, in bytes */
	uint8_t     *data;  /* Pointer to len bytes of data. */
} protocol_data_unit;

struct protocol_context;

/**
  protocol_handler
 */
typedef int (*protocol_handler)(struct protocol_context*);

struct protocol_context {
	SSL                *io;        /* IO stream to read from / write to */

	protocol_data_unit  send_pdu;  /* PDU to send to the other side */
	protocol_data_unit  recv_pdu;  /* PDU received from other side */
	protocol_handler    next;
};

int pdu_encode_NOOP(protocol_data_unit *pdu);
int pdu_encode_ERROR(protocol_data_unit *pdu, uint16_t err_code, const char *str);
int pdu_encode_ACK(protocol_data_unit *pdu);
int pdu_encode_BYE(protocol_data_unit *pdu);
int pdu_encode_GET_VERSION(protocol_data_unit *pdu);
int pdu_encode_SEND_VERSION(protocol_data_unit *pdu, uint32_t version);
int pdu_encode_GET_POLICY(protocol_data_unit *pdu);
int pdu_encode_SEND_POLICY(protocol_data_unit *pdu, const char *policy);

int pdu_receive(struct protocol_context *ctx);
int pdu_send(struct protocol_context *ctx);

void init_openssl(void);

int proto_init(struct protocol_context *pctx, SSL *io);

int server_dispatch(struct protocol_context *pctx);

int client_query(struct protocol_context *pctx);
int client_retrieve(struct protocol_context *pctx, char **data, size_t *len);
int client_report(struct protocol_context *pctx, char *data, size_t len);
int client_bye(struct protocol_context *pctx);

#endif
