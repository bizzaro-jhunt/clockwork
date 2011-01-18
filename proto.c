#include "clockwork.h"

#include <arpa/inet.h>
#include <netdb.h>

#include "proto.h"
#include "policy.h"
#include "stringlist.h"

#define FILE_DATA_BLOCK_SIZE 8192

static int pdu_allocate(protocol_data_unit *pdu, uint16_t op, uint16_t len)
{
	assert(pdu);

	pdu->op = op;

	free(pdu->data);
	pdu->data = xmalloc(len);
	pdu->len = len;

	return 0;
}

static void pdu_deallocate(protocol_data_unit *pdu)
{
	assert(pdu);

	free(pdu->data);
	pdu->data = NULL;
}

int pdu_send_ERROR(protocol_session *session, uint16_t err_code, const uint8_t *str, size_t len)
{
	assert(session);

	protocol_data_unit *pdu = SEND_PDU(session);

	err_code = htons(err_code);

	if (pdu_allocate(pdu, PROTOCOL_OP_ERROR, len + sizeof(err_code)) < 0) {
		return -1;
	}

	memcpy(pdu->data, &err_code, sizeof(err_code));
	memcpy(pdu->data + sizeof(err_code), str, len);

	return pdu_write(session->io, SEND_PDU(session));
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
	*str = xmalloc(my_len * sizeof(uint8_t));
	memcpy(*str, pdu->data + sizeof(*err_code), my_len);
	if (len) {
		*len = my_len;
	}

	return 0;
}

int pdu_send_ACK(protocol_session *session)
{
	assert(session);

	protocol_data_unit *pdu = SEND_PDU(session);

	if (pdu_allocate(pdu, PROTOCOL_OP_ACK, 0) < 0) {
		return -1;
	}

	return pdu_write(session->io, SEND_PDU(session));
}

int pdu_send_BYE(protocol_session *session)
{
	assert(session);

	protocol_data_unit *pdu = SEND_PDU(session);

	if (pdu_allocate(pdu, PROTOCOL_OP_BYE, 0) < 0) {
		return -1;
	}

	return pdu_write(session->io, SEND_PDU(session));
}

int pdu_send_GET_POLICY(protocol_session *session, const struct hash *facts)
{
	assert(session);
	assert(facts);

	protocol_data_unit *pdu = SEND_PDU(session);
	stringlist *list = stringlist_new(NULL);
	struct hash_cursor cur;

	char *key, *value, kvp[256];

	char *buf;
	size_t len;

	for_each_key_value(facts, &cur, key, value) {
		snprintf(kvp, 256, "%s=%s\n", key, value);
		stringlist_add(list, kvp); /* stringlist_add strdup's */
	}

	buf = stringlist_join(list, "");
	stringlist_free(list);
	len = strlen(buf);

	if (pdu_allocate(pdu, PROTOCOL_OP_GET_POLICY, len) < 0) {
		return -1;
	}

	memcpy(pdu->data, buf, len);

	return pdu_write(session->io, SEND_PDU(session));
}

int pdu_decode_GET_POLICY(protocol_data_unit *pdu, struct hash *facts)
{
	assert(pdu);
	assert(pdu->op == PROTOCOL_OP_GET_POLICY);
	assert(facts);

	size_t i;
	char *k, *v;

	stringlist *lines = stringlist_split((char*)pdu->data, pdu->len, "\n");
	if (!lines) {
		return -1;
	}

	for (i = 0; i < lines->num; i++) {
		if (fact_parse(lines->strings[i], &k, &v) == 0) {
			hash_set(facts, k, v);
			free(k);
		}
	}

	return 0;
}

int pdu_send_SEND_POLICY(protocol_session *session, const struct policy *policy)
{
	assert(session);
	assert(policy);

	protocol_data_unit *pdu = SEND_PDU(session);
	char *packed;
	size_t len;

	packed = policy_pack(policy);
	if (!packed) {
		return -1;
	}

	len = strlen(packed);

	if (pdu_allocate(pdu, PROTOCOL_OP_SEND_POLICY, len) < 0) {
		free(packed);
		return -1;
	}

	memcpy(pdu->data, packed, len);
	free(packed);

	return pdu_write(session->io, pdu);
}

int pdu_decode_SEND_POLICY(protocol_data_unit *pdu, struct policy **policy)
{
	assert(pdu);
	assert(pdu->op == PROTOCOL_OP_SEND_POLICY);
	assert(policy);

	*policy = policy_unpack((char*)pdu->data);
	if (*policy) {
		return 0;
	} else {
		return -1;
	}
}

int pdu_send_GET_FILE(protocol_session *session, sha1 *checksum)
{
	assert(session);
	assert(checksum);

	protocol_data_unit *pdu = SEND_PDU(session);

	if (pdu_allocate(pdu, PROTOCOL_OP_GET_FILE, SHA1_HEX_DIGEST_SIZE) < 0) {
		return -1;
	}

	memcpy(pdu->data, checksum->hex, pdu->len);

	return pdu_write(session->io, pdu);
}

int pdu_decode_GET_FILE(protocol_data_unit *pdu, sha1 *checksum)
{
	assert(pdu);
	assert(pdu->op == PROTOCOL_OP_GET_FILE);
	assert(pdu->len == SHA1_HEX_DIGEST_SIZE);
	assert(checksum);

	char hex[SHA1_HEX_DIGEST_SIZE + 1] = {0};
	memcpy(hex, pdu->data, SHA1_HEX_DIGEST_SIZE);

	sha1_init(checksum, hex);

	return 0;
}

int pdu_send_FILE_DATA(protocol_session *session, int srcfd)
{
	assert(session);

	protocol_data_unit *pdu = SEND_PDU(session);

	char chunk[FILE_DATA_BLOCK_SIZE];
	size_t len = 0, nread = 0;
	int write_status;

	do {
		nread = read(srcfd, chunk + len, FILE_DATA_BLOCK_SIZE - len);
		len += nread;
	} while (nread != 0 && len < FILE_DATA_BLOCK_SIZE);

	if (pdu_allocate(pdu, PROTOCOL_OP_FILE_DATA, len) < 0) {
		return -1;
	}

	memcpy(pdu->data, chunk, len); /* note: len could be 0 */

	write_status = pdu_write(session->io, pdu);
	if (write_status != 0) {
		return write_status;
	}

	return len;
}

int pdu_decode_FILE_DATA(protocol_data_unit *pdu, int dstfd)
{
	assert(pdu);
	assert(pdu->op == PROTOCOL_OP_FILE_DATA);

	size_t len = pdu->len;

	write(dstfd, pdu->data, len);
	return len;
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

	DEVELOPER("pdu_read:  OP:%04u; LEN:%04u", pdu->op, pdu->len);
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

	DEVELOPER("pdu_write: OP:%04u; LEN:%04u", pdu->op, pdu->len);
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
		DEVELOPER("pdu_receive: pdu_read returned %i", rc);
		return -1;
	}

	/* handle error response */
	if (pdu->op == PROTOCOL_OP_ERROR) {
		free(session->errstr);
		if (pdu_decode_ERROR(pdu, &(session->errnum), &(session->errstr), NULL) < 0) {
			perror("Unable to decode ERROR PDU");
			return -1;
		}

		DEVELOPER("Received an ERROR: %u - %s", session->errnum, session->errstr);
		return -1;
	}

	return 0;
}

/**********************************************************/

void protocol_ssl_init(void)
{
	if (!SSL_library_init()) {
		CRITICAL("protocol_ssl_init: Failed to initialize OpenSSL");
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
		ERROR("Unable to get certificate in SSL_get_peer_certificate (for host '%s')", host);
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

void protocol_ssl_backtrace(void)
{
	unsigned long e;

	while ( (e = ERR_get_error()) != 0 ) {
		DEBUG("SSL: %s", ERR_reason_error_string(e));
	}
}

int protocol_reverse_lookup_verify(int sockfd, char *buf, size_t len)
{
	struct sockaddr_in ipv4;
	socklen_t ipv4_len;

	ipv4_len = sizeof(ipv4);
	memset(&ipv4, 0, ipv4_len);

	if (getpeername(sockfd, (struct sockaddr*)(&ipv4), &ipv4_len) != 0
	 || getnameinfo((struct sockaddr*)(&ipv4), ipv4_len, buf, len, NULL, 0, NI_NAMEREQD) != 0) {
		return -1;
	}

	return 0;
}
