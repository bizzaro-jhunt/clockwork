#include <assert.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "proto.h"
#include "net.h"

static int pdu_allocate(protocol_data_unit *pdu, uint16_t op, uint16_t len);

static int server_main(struct protocol_context *pctx);

/**********************************************************/

static int pdu_allocate(protocol_data_unit *pdu, uint16_t op, uint16_t len)
{
	assert(pdu);

	pdu->op = op;

	if (pdu->data) {
		free(pdu->data);
	}

	pdu->data = malloc(len);
	if (!pdu->data) {
		return -1;
	}

	pdu->len = len;

	return 0;
}

static int server_main(struct protocol_context *pctx)
{
	char errbuf[256] = {0};

	pdu_receive(pctx);
	switch (pctx->recv_pdu.op) {

	case PROTOCOL_OP_NOOP:
		if (pdu_encode_NOOP(&pctx->send_pdu) < 0) {
			fprintf(stderr, "Unable to encode NOOP\n");
			exit(42);
		}
		pdu_send(pctx);
		break;

	case PROTOCOL_OP_BYE:
		if (pdu_encode_ACK(&pctx->send_pdu) < 0) {
			fprintf(stderr, "Unable to encode ACK\n");
			exit(42);
		}
		pdu_send(pctx);
		return -1;

	case PROTOCOL_OP_GET_VERSION:
		if (pdu_encode_SEND_VERSION(&pctx->send_pdu, 452356) < 0) {
			fprintf(stderr, "Unable to encode SEND_VERSION\n");
			exit(42);
		}
		pdu_send(pctx);
		return 0;

	case PROTOCOL_OP_GET_POLICY:
		if (pdu_encode_SEND_POLICY(&pctx->send_pdu, "POLICY\nPOLICY\nPOLICY") < 0) {
			fprintf(stderr, "Unable to encode GET_VERSION\n");
			exit(42);
		}
		pdu_send(pctx);
		return 0;

	case PROTOCOL_OP_PUT_REPORT:
		if (pdu_encode_ACK(&pctx->send_pdu) < 0) {
			fprintf(stderr, "Unable to encode ACK\n");
			exit(42);
		}
		pdu_send(pctx);
		break;

	default:
		snprintf(errbuf, 256, "Unrecognized PDU OP: %u", pctx->recv_pdu.op);
		if (pdu_encode_ERROR(&pctx->send_pdu, 405, errbuf) < 0) {
			fprintf(stderr, "Unable to encode ERROR\n");
			exit(42);
		}
		pdu_send(pctx);
		return -1;
	}

#if 0
	} else if (strcmp(pctx->line, "REPORT") == 0) {
		network_printf(pctx->nbuf, "301 Go Ahead\r\n");
		__proto_readline(pctx);
		while (strcmp(pctx->line, "DONE") != 0) {
			__proto_readline(pctx);
		}
		network_printf(pctx->nbuf, "200 OK\r\n");
		pctx->next = server_main;
		return 0;

	}
#endif
}

int pdu_encode_NOOP(protocol_data_unit *pdu)
{
	return pdu_allocate(pdu, PROTOCOL_OP_NOOP, 0);
}

int pdu_encode_ERROR(protocol_data_unit *pdu, uint16_t err_code, const char *str)
{
	assert(pdu);

	uint16_t len = strlen(str);
	err_code = htons(err_code);

	if (pdu_allocate(pdu, PROTOCOL_OP_ERROR, len + sizeof(err_code)) < 0) {
		return -1;
	}

	memcpy(pdu->data, &err_code, sizeof(err_code));
	memcpy(pdu->data + sizeof(err_code), str, len);

	return 0;
}

int pdu_encode_ACK(protocol_data_unit *pdu)
{
	return pdu_allocate(pdu, PROTOCOL_OP_ACK, 0);
}

int pdu_encode_BYE(protocol_data_unit *pdu)
{
	return pdu_allocate(pdu, PROTOCOL_OP_BYE, 0);
}

int pdu_encode_GET_VERSION(protocol_data_unit *pdu)
{
	return pdu_allocate(pdu, PROTOCOL_OP_GET_VERSION, 0);
}

int pdu_encode_SEND_VERSION(protocol_data_unit *pdu, uint32_t version)
{
	assert(pdu);

	version = htonl(version);

	if (pdu_allocate(pdu, PROTOCOL_OP_SEND_VERSION, sizeof(version)) < 0) {
		return -1;
	}

	memcpy(pdu->data, &version, sizeof(version));

	return 0;
}

int pdu_encode_GET_POLICY(protocol_data_unit *pdu)
{
	return pdu_allocate(pdu, PROTOCOL_OP_GET_POLICY, 0);
}

int pdu_encode_SEND_POLICY(protocol_data_unit *pdu, const char *policy)
{
	assert(pdu);

	uint32_t len = strlen(policy);

	if (pdu_allocate(pdu, PROTOCOL_OP_SEND_POLICY, len) < 0) {
		return -1;
	}

	memcpy(pdu->data, policy, len);

	return 0;
}

int pdu_receive(struct protocol_context *ctx)
{
	assert(ctx);

	protocol_data_unit *pdu = &(ctx->recv_pdu);
	uint16_t op, len;
	int nread;

	nread = SSL_read(ctx->io, &op, sizeof(op));
	if (nread <= 0) {
		fprintf(stderr, "pdu_receive: got %i from SSL_read\n", nread);
		exit(42);
	}
	op = ntohs(op);

	nread = SSL_read(ctx->io, &len, sizeof(len));
	if (nread <= 0) {
		fprintf(stderr, "pdu_receive: got %i from SSL_read\n", nread);
		exit(42);
	}
	len = ntohs(len);

	if (pdu_allocate(pdu, op, len) < 0) {
		perror("pdu_receive pdu_allocate");
		exit(42);
	}

	fprintf(stderr, "pdu_receive: OP:%u;LEN:%u\n", pdu->op, pdu->len);

	if (len > 0) {
		nread = SSL_read(ctx->io, pdu->data, pdu->len);
		if (nread <= 0) {
			fprintf(stderr, "pdu_receive: got %i from SSL_read\n", nread);
			exit(42);
		}
	}

	return 0;
}

int pdu_send(struct protocol_context *ctx)
{
	assert(ctx);

	protocol_data_unit *pdu = &(ctx->send_pdu);
	int nwritten;
	uint16_t op, len;

	fprintf(stderr, "pdu_send: OP:%u;LEN:%u\n", pdu->op, pdu->len);

	op  = htons(pdu->op);
	len = htons(pdu->len);

	/* Write op to the wire */
	nwritten = SSL_write(ctx->io, &op, sizeof(op));
	if (nwritten != sizeof(op)) {
		fprintf(stderr, "pdu_send: got %i from SSL_write\n", nwritten);
		exit(42);
	}

	/* Write len to the wire */
	nwritten = SSL_write(ctx->io, &len, sizeof(len));
	if (nwritten != sizeof(len)) {
		fprintf(stderr, "pdu_send: got %i from SSL_write\n", nwritten);
		exit(42);
	}

	if (pdu->len > 0) {
		/* Write payload to the wire */
		nwritten = SSL_write(ctx->io, pdu->data, pdu->len);
		if (nwritten != pdu->len) {
			fprintf(stderr, "pdu_send: got %i from SSL_write\n", nwritten);
			exit(42);
		}
	}

	return 0;
}

/**********************************************************/

void init_openssl(void)
{
	if (!SSL_library_init()) {
		fprintf(stderr, "init_openssl: Failed to initialize OpenSSL\n");
		exit(1);
	}
	SSL_load_error_strings();
}

int proto_init(struct protocol_context *pctx, SSL *io)
{
	pctx->io = io;
	memset(&pctx->send_pdu, 0, sizeof(protocol_data_unit));
	memset(&pctx->recv_pdu, 0, sizeof(protocol_data_unit));
	pctx->next = NULL;

	init_openssl();

	return 0;
}

int server_dispatch(struct protocol_context *pctx)
{
	fprintf(stderr, "  server process started\n");

	pctx->next = server_main;
	while ( pctx->next && (*(pctx->next))(pctx) == 0 )
		;

	return 0;
}

int client_query(struct protocol_context *pctx)
{
	if (pdu_encode_GET_POLICY(&pctx->send_pdu) < 0) {
		perror("client_query");
		return -1;
	}
	if (pdu_send(pctx) < 0) {
		perror("client_query");
		return -1;
	}

	pdu_receive(pctx);
	return 200; /* FIXME: temporary */
}

int client_retrieve(struct protocol_context *pctx, char **data, size_t *n)
{
}

int client_report(struct protocol_context *pctx, char *data, size_t n)
{
}

int client_bye(struct protocol_context *pctx)
{
	if (pdu_encode_BYE(&pctx->send_pdu) < 0) {
		perror("client_bye");
		return -1;
	}
	if (pdu_send(pctx) < 0) {
		perror("client_bye");
		return -1;
	}

	pdu_receive(pctx);
	return 0;
}

