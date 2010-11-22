#include <assert.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <arpa/inet.h>

#include "proto.h"


#define SEND_PDU(session_ptr) (&(session_ptr)->send_pdu)
#define RECV_PDU(session_ptr) (&(session_ptr)->recv_pdu)

static int pdu_allocate(protocol_data_unit *pdu, uint16_t op, uint16_t len);

/**********************************************************/

static int pdu_allocate(protocol_data_unit *pdu, uint16_t op, uint16_t len)
{
	assert(pdu);

	pdu->op = op;

	free(pdu->data);
	pdu->data = malloc(len);
	if (!pdu->data) {
		return -1;
	}

	pdu->len = len;

	return 0;
}

static void pdu_deallocate(protocol_data_unit *pdu)
{
	assert(pdu);

	free(pdu->data);
	pdu->data = NULL;
}

int server_dispatch(protocol_session *session)
{
	char errbuf[256] = {0};

	for (;;) {
		pdu_receive(session);
		switch (session->recv_pdu.op) {

		case PROTOCOL_OP_BYE:
			return 0;

		case PROTOCOL_OP_GET_POLICY:
			if (pdu_send_SEND_POLICY(session, (uint8_t*)"POLICY\nPOLICY\nPOLICY", 20) < 0) {
				fprintf(stderr, "Unable to send SEND_POLICY\n");
				exit(42);
			}
			break;

		case PROTOCOL_OP_PUT_REPORT:
			if (pdu_send_ACK(session) < 0) {
				fprintf(stderr, "Unable to send ACK\n");
				exit(42);
			}
			break;

		default:
			snprintf(errbuf, 256, "Unrecognized PDU OP: %u", session->recv_pdu.op);
			if (pdu_send_ERROR(session, 405, (uint8_t*)errbuf, strlen(errbuf)) < 0) {
				fprintf(stderr, "Unable to send ERROR\n");
				exit(42);
			}
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

int pdu_send_ERROR(protocol_session *session, uint16_t err_code, const uint8_t *str, size_t len)
{
	assert(session);

	int rc = pdu_encode_ERROR(SEND_PDU(session), err_code, str, len);
	if (rc < 0) {
		return 0;
	}

	return pdu_write(session->io, SEND_PDU(session));
}

int pdu_encode_ERROR(protocol_data_unit *pdu, uint16_t err_code, const uint8_t *str, size_t len)
{
	assert(pdu);

	err_code = htons(err_code);

	if (pdu_allocate(pdu, PROTOCOL_OP_ERROR, len + sizeof(err_code)) < 0) {
		return -1;
	}

	memcpy(pdu->data, &err_code, sizeof(err_code));
	memcpy(pdu->data + sizeof(err_code), str, len);

	return 0;
}

int pdu_decode_ERROR(protocol_data_unit *pdu, uint16_t *err_code, uint8_t **str, size_t *len)
{
	assert(pdu);
	assert(pdu->op == PROTOCOL_OP_ERROR);
	assert(err_code);
	assert(str);
	/* len parameter is optional */

	size_t my_len;

	memcpy(err_code, pdu->data, sizeof(*err_code));
	*err_code = ntohs(*err_code);

	my_len = pdu->len - sizeof(*err_code) + 1;
	*str = calloc(my_len, sizeof(uint8_t));
	if (!str) {
		return -1;
	}

	memcpy(*str, pdu->data + sizeof(*err_code), my_len);
	if (len) {
		*len = my_len;
	}

	return 0;
}

int pdu_send_ACK(protocol_session *session)
{
	assert(session);

	int rc = pdu_encode_ACK(SEND_PDU(session));
	if (rc < 0) {
		return 0;
	}

	return pdu_write(session->io, SEND_PDU(session));
}

int pdu_encode_ACK(protocol_data_unit *pdu)
{
	return pdu_allocate(pdu, PROTOCOL_OP_ACK, 0);
}

int pdu_decode_ACK(protocol_data_unit *pdu)
{
	assert(pdu);
	assert(pdu->op == PROTOCOL_OP_ACK);

	/* no special processing for ACK PDUs */

	return 0;
}

int pdu_send_BYE(protocol_session *session)
{
	assert(session);

	int rc = pdu_encode_BYE(SEND_PDU(session));
	if (rc < 0) {
		return 0;
	}

	return pdu_write(session->io, SEND_PDU(session));
}

int pdu_encode_BYE(protocol_data_unit *pdu)
{
	return pdu_allocate(pdu, PROTOCOL_OP_BYE, 0);
}

int pdu_decode_BYE(protocol_data_unit *pdu)
{
	assert(pdu);
	assert(pdu->op == PROTOCOL_OP_BYE);

	/* no special processing for BYE PDUs */

	return 0;
}

int pdu_send_GET_POLICY(protocol_session *session)
{
	assert(session);

	int rc = pdu_encode_GET_POLICY(SEND_PDU(session));
	if (rc < 0) {
		return 0;
	}

	return pdu_write(session->io, SEND_PDU(session));
}

int pdu_encode_GET_POLICY(protocol_data_unit *pdu)
{
	return pdu_allocate(pdu, PROTOCOL_OP_GET_POLICY, 0);
}

int pdu_decode_GET_POLICY(protocol_data_unit *pdu)
{
	assert(pdu);
	assert(pdu->op == PROTOCOL_OP_GET_POLICY);

	/* no special processing for GET_POLICY PDUs */

	return 0;
}

int pdu_send_SEND_POLICY(protocol_session *session, const uint8_t *policy, size_t len)
{
	assert(session);

	int rc = pdu_encode_SEND_POLICY(SEND_PDU(session), policy, len);
	if (rc < 0) {
		return 0;
	}

	return pdu_write(session->io, SEND_PDU(session));
}

int pdu_encode_SEND_POLICY(protocol_data_unit *pdu, const uint8_t *policy, size_t len)
{
	assert(pdu);
	assert(policy);

	if (pdu_allocate(pdu, PROTOCOL_OP_SEND_POLICY, len) < 0) {
		return -1;
	}

	memcpy(pdu->data, policy, len);
	return 0;
}

int pdu_decode_SEND_POLICY(protocol_data_unit *pdu, uint8_t **policy, size_t *len)
{
	assert(pdu);
	assert(pdu->op == PROTOCOL_OP_SEND_POLICY);
	assert(policy);
	/* len is optional */

	/* FIXME: implement pdu_decode_SEND_POLICY */

	return 0;
}

int pdu_read(SSL *io, protocol_data_unit *pdu)
{
	assert(io);
	assert(pdu);

	uint16_t op, len;
	int nread;

	nread = SSL_read(io, &op, sizeof(op));
	if (nread <= 0) {
		return -1; /* error reading header */
	}
	op = ntohs(op);

	nread = SSL_read(io, &len, sizeof(len));
	if (nread <= 0) {
		return -1; /* error reading header */
	}
	len = ntohs(len);

	if (pdu_allocate(pdu, op, len) < 0) {
		return -2; /* error allocating pdu */
	}

	if (len > 0) {
		nread = SSL_read(io, pdu->data, pdu->len);
		if (nread <= 0) {
			return -3; /* error reading payload */
		}
	}

	fprintf(stderr, "pdu_read:  OP:%04u; LEN:%04u\n", pdu->op, pdu->len);
	return 0;
}

int pdu_write(SSL *io, protocol_data_unit *pdu)
{
	assert(io);
	assert(pdu);

	uint16_t op, len;
	int nwritten;

	op  = htons(pdu->op);
	len = htons(pdu->len);

	nwritten = SSL_write(io, &op, sizeof(op));
	if (nwritten != sizeof(op)) {
		return -1; /* error writing header */
	}

	nwritten = SSL_write(io, &len, sizeof(len));
	if (nwritten != sizeof(len)) {
		return -1; /* error writing header */
	}

	if (pdu->len > 0) {
		nwritten = SSL_write(io, pdu->data, pdu->len);
		if (nwritten != pdu->len) {
			return -3; /* error writing payload */
		}
	}

	fprintf(stderr, "pdu_write: OP:%04u; LEN:%04u\n", pdu->op, pdu->len);
	return 0;
}

/**
  Returns 0 on success, or < 0 on failure. Failure includes
    application-level issues (the ERROR PDU).
 */
int pdu_receive(protocol_session *session)
{
	assert(session);

	protocol_data_unit *pdu = RECV_PDU(session);
	int rc;

	rc = pdu_read(session->io, pdu);
	if (rc < 0) {
		fprintf(stderr, "pdu_receive: pdu_read returned %i\n", rc);
		exit(42);
	}

	/* handle error response */
	if (pdu->op == PROTOCOL_OP_ERROR) {
		free(session->errstr);
		if (pdu_decode_ERROR(pdu, &(session->errnum), &(session->errstr), NULL) < 0) {
			perror("Unable to decode ERROR PDU");
			exit(42);
		}

		fprintf(stderr, "Received an ERROR: %u - %s\n", session->errnum, session->errstr);
		return -1;
	}

	return 0;
}

/**********************************************************/

void protocol_ssl_init(void)
{
	if (!SSL_library_init()) {
		fprintf(stderr, "protocol_ssl_init: Failed to initialize OpenSSL\n");
		exit(1);
	}
	SSL_load_error_strings();
	RAND_load_file("/dev/urandom", 1024);
}

int protocol_session_init(protocol_session *session, SSL *io)
{
	session->io = io;

	memset(SEND_PDU(session), 0, sizeof(protocol_data_unit));
	memset(RECV_PDU(session), 0, sizeof(protocol_data_unit));

	session->errnum = 0;
	session->errstr = NULL;

	return 0;
}

int protocol_session_deinit(protocol_session *session)
{
	session->io = NULL;

	pdu_deallocate(SEND_PDU(session));
	memset(SEND_PDU(session), 0, sizeof(protocol_data_unit));

	pdu_deallocate(RECV_PDU(session));
	memset(RECV_PDU(session), 0, sizeof(protocol_data_unit));

	session->errnum = 0;
	free(session->errstr);

	return 0;
}

long protocol_ssl_verify_peer(SSL *ssl, const char *host)
{
	X509 *cert;
	X509_NAME *subj;
	char data[256];
	int extcount;
	int ok = 0;

	if (!(cert = SSL_get_peer_certificate(ssl)) || !host) {
		fprintf(stderr, "Unable to get certificate in SSL_get_peer_certificate (for host '%s')\n", host);
		goto err_occurred;
	}

	if ((extcount = X509_get_ext_count(cert)) > 0) {
		int i;

		for (i = 0; i < extcount; i++) {
			const char *extstr;
			X509_EXTENSION *ext;

			ext = X509_get_ext(cert, i);
			extstr = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));

			if (strcmp(extstr, "subjectAltName") == 0) {
				int j;
				const unsigned char *data;
				STACK_OF(CONF_VALUE) *val;
				CONF_VALUE *nval;
				X509V3_EXT_METHOD *meth;

				if (!(meth = X509V3_EXT_get(ext))) {
					break;
				}
				data = ext->value->data;
				val = meth->i2v(meth,
				                meth->d2i(NULL, &data, ext->value->length),
				                NULL);
				for (j = 0; j < sk_CONF_VALUE_num(val); j++) {
					nval = sk_CONF_VALUE_value(val, j);
					if (!strcmp(nval->name, "DNS") && !strcmp(nval->value, host)) {
						ok = 1;
						break;
					}
				}
			}
			if (ok) {
				break;
			}
		}
	}

	if (!ok && (subj = X509_get_subject_name(cert)) &&
	    X509_NAME_get_text_by_NID(subj, NID_commonName, data, 256) > 0) {
		data[255] = '\0';
		if (strcasecmp(data, host) != 0) {
			goto err_occurred;
		}
	}

	X509_free(cert);
	return SSL_get_verify_result(ssl);

err_occurred:
	X509_free(cert);
	return X509_V_ERR_APPLICATION_VERIFICATION;
}

SSL_CTX* protocol_ssl_default_context(const char *ca_cert_file, const char *cert_file, const char *key_file)
{
	assert(ca_cert_file);
	assert(cert_file);
	assert(key_file);

	SSL_CTX *ctx;

	ctx = SSL_CTX_new(TLSv1_method());
	if (!ctx) {
		return NULL;
	}
	if (SSL_CTX_load_verify_locations(ctx, ca_cert_file, NULL) != 1
	 || SSL_CTX_use_certificate_chain_file(ctx, cert_file) != 1
	 || SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) != 1) {
		/* free */
		return NULL;
	}

	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	SSL_CTX_set_verify_depth(ctx, 4);
	return ctx;
}

int client_disconnect(protocol_session *session)
{
	if (pdu_send_BYE(session) < 0) {
		perror("client_disconnect");
		return -1;
	}

	pdu_receive(session);
	return 0;
}

