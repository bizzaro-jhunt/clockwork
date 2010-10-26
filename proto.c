#include <assert.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "proto.h"
#include "net.h"

static int pdu_allocate(protocol_data_unit *pdu, uint16_t op, uint16_t len);

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

int server_dispatch(protocol_session *session)
{
	char errbuf[256] = {0};

	for (;;) {
		pdu_receive(session);
		switch (session->recv_pdu.op) {

		case PROTOCOL_OP_NOOP:
			if (pdu_encode_NOOP(&session->send_pdu) < 0) {
				fprintf(stderr, "Unable to encode NOOP\n");
				exit(42);
			}
			pdu_send(session);
			break;

		case PROTOCOL_OP_BYE:
			if (pdu_encode_ACK(&session->send_pdu) < 0) {
				fprintf(stderr, "Unable to encode ACK\n");
				exit(42);
			}
			pdu_send(session);
			return 0;

		case PROTOCOL_OP_GET_VERSION:
			if (pdu_encode_SEND_VERSION(&session->send_pdu, 452356) < 0) {
				fprintf(stderr, "Unable to encode SEND_VERSION\n");
				exit(42);
			}
			pdu_send(session);
			break;

		case PROTOCOL_OP_GET_POLICY:
			if (pdu_encode_SEND_POLICY(&session->send_pdu, "POLICY\nPOLICY\nPOLICY") < 0) {
				fprintf(stderr, "Unable to encode GET_VERSION\n");
				exit(42);
			}
			pdu_send(session);
			break;

		case PROTOCOL_OP_PUT_REPORT:
			if (pdu_encode_ACK(&session->send_pdu) < 0) {
				fprintf(stderr, "Unable to encode ACK\n");
				exit(42);
			}
			pdu_send(session);
			break;

		default:
			snprintf(errbuf, 256, "Unrecognized PDU OP: %u", session->recv_pdu.op);
			if (pdu_encode_ERROR(&session->send_pdu, 405, errbuf) < 0) {
				fprintf(stderr, "Unable to encode ERROR\n");
				exit(42);
			}
			pdu_send(session);
			return -1;
		}

#if 0
	} else if (strcmp(session->line, "REPORT") == 0) {
		network_printf(session->nbuf, "301 Go Ahead\r\n");
		__proto_readline(session);
		while (strcmp(session->line, "DONE") != 0) {
			__proto_readline(session);
		}
		network_printf(session->nbuf, "200 OK\r\n");
		return 0;

	}
#endif
	} /* for (;;) */
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

int pdu_receive(protocol_session *session)
{
	assert(session);

	protocol_data_unit *pdu = &(session->recv_pdu);
	uint16_t op, len;
	int nread;

	nread = SSL_read(session->io, &op, sizeof(op));
	if (nread <= 0) {
		fprintf(stderr, "pdu_receive: got %i from SSL_read\n", nread);
		exit(42);
	}
	op = ntohs(op);

	nread = SSL_read(session->io, &len, sizeof(len));
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
		nread = SSL_read(session->io, pdu->data, pdu->len);
		if (nread <= 0) {
			fprintf(stderr, "pdu_receive: got %i from SSL_read\n", nread);
			exit(42);
		}
	}

	return 0;
}

int pdu_send(protocol_session *session)
{
	assert(session);

	protocol_data_unit *pdu = &(session->send_pdu);
	int nwritten;
	uint16_t op, len;

	fprintf(stderr, "pdu_send: OP:%u;LEN:%u\n", pdu->op, pdu->len);

	op  = htons(pdu->op);
	len = htons(pdu->len);

	/* Write op to the wire */
	nwritten = SSL_write(session->io, &op, sizeof(op));
	if (nwritten != sizeof(op)) {
		fprintf(stderr, "pdu_send: got %i from SSL_write\n", nwritten);
		exit(42);
	}

	/* Write len to the wire */
	nwritten = SSL_write(session->io, &len, sizeof(len));
	if (nwritten != sizeof(len)) {
		fprintf(stderr, "pdu_send: got %i from SSL_write\n", nwritten);
		exit(42);
	}

	if (pdu->len > 0) {
		/* Write payload to the wire */
		nwritten = SSL_write(session->io, pdu->data, pdu->len);
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

int protocol_session_init(protocol_session *session, SSL *io)
{
	session->io = io;
	memset(&session->send_pdu, 0, sizeof(protocol_data_unit));
	memset(&session->recv_pdu, 0, sizeof(protocol_data_unit));

	init_openssl();

	return 0;
}

int client_query(protocol_session *session)
{
	if (pdu_encode_GET_POLICY(&session->send_pdu) < 0) {
		perror("client_query");
		return -1;
	}
	if (pdu_send(session) < 0) {
		perror("client_query");
		return -1;
	}

	pdu_receive(session);
	return 200; /* FIXME: temporary */
}

int client_retrieve(protocol_session *session, char **data, size_t *n)
{
}

int client_report(protocol_session *session, char *data, size_t n)
{
}

int client_bye(protocol_session *session)
{
	if (pdu_encode_BYE(&session->send_pdu) < 0) {
		perror("client_bye");
		return -1;
	}
	if (pdu_send(session) < 0) {
		perror("client_bye");
		return -1;
	}

	pdu_receive(session);
	return 0;
}

