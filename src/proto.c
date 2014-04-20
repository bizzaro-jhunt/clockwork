/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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

#include "clockwork.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>

#include "proto.h"
#include "policy.h"

#define FILE_DATA_BLOCK_SIZE 8192

static const char *protocol_op_names[] = {
	NULL,
	"ERROR",
	"HELLO",
	"FACTS",
	"POLICY",
	"FILE",
	"DATA",
	"REPORT",
	"BYE",

	"GET_CERT",
	"SEND_CERT",
	NULL
};
#define protocol_op_max 10

static int pdu_allocate(struct pdu *pdu, uint16_t op, uint16_t len)
{
	assert(pdu); // LCOV_EXCL_LINE

	pdu->op = op;

	free(pdu->data);
	pdu->data = xmalloc(len+1);
	pdu->len = len;

	return 0;
}

static void pdu_deallocate(struct pdu *pdu)
{
	assert(pdu); // LCOV_EXCL_LINE

	free(pdu->data);
	pdu->data = NULL;
}

static void pdu_dump(struct pdu *pdu)
{
	assert(pdu); // LCOV_EXCL_LINE

	/* no point in going any further if we aren't debugging... */
	if (log_level() < LOG_LEVEL_DEBUG) { return; }

/*
Op:   XXX (xx)
Len:  xxx
Data: xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx
      xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx
*/

	char prefix[] = "Data:";
	char data[16][4];

	size_t i, j, n;
	size_t lines = pdu->len / 16 + (pdu->len % 16 ? 1 : 0);

	memset(data, '\0', 16*4);

	DEBUG("Op:   %s (%04u)", protocol_op_name(pdu->op), pdu->op);
	DEBUG("Len:  %u", pdu->len);

	n = 0;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < 16; j++, n++) {
			if (n >= pdu->len) {
				memcpy(data[j], "", 2);
			} else {
				if (pdu->data[n] == '\n') {
					snprintf(data[j], 4, " \\n");
				} else if (pdu->data[n] == ' ') {
					snprintf(data[j], 4, " sp");
				} else if (isprint(pdu->data[n])) {
					snprintf(data[j], 4, "  %c", pdu->data[n]);
				} else {
					snprintf(data[j], 4, " %02x", pdu->data[n]);
				}
			}
		}
		DEBUG("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", prefix,
		      data[0],  data[1],  data[2],  data[3],
		      data[4],  data[5],  data[6],  data[7],
		      data[8],  data[9],  data[10], data[11],
		      data[12], data[13], data[14], data[15]);
		memset(prefix, ' ', 5);
	}
	DEBUG("");
}

int pdu_send_simple(struct session *session, enum proto_op op)
{
	assert(session); // LCOV_EXCL_LINE

	if (pdu_allocate(SEND_PDU(session), op, 0) < 0) {
		return -1;
	}

	DEBUG("SEND %s (op:%u) - (empty payload)", protocol_op_name(op), op);
	return pdu_write(session->io, SEND_PDU(session));
}

/**
  Send an ERROR PDU to the remote end.

  $err_code contains the error code, and $str is the
  descriptive error message.

  On success, returns 0.  On failure, returns non-zero.
 */
int pdu_send_ERROR(struct session *session, uint16_t err_code, const char *str)
{
	assert(session); // LCOV_EXCL_LINE

	size_t len = strlen(str);
	struct pdu *pdu = SEND_PDU(session);


	DEBUG("SEND ERROR (op:%u) - %u %s", PROTOCOL_OP_ERROR, err_code, str);

	if (pdu_allocate(pdu, PROTOCOL_OP_ERROR, len + sizeof(err_code)) < 0) {
		return -1;
	}

	err_code = htons(err_code);
	memcpy(pdu->data, &err_code, sizeof(err_code));
	memcpy(pdu->data + sizeof(err_code), str, len);

	return pdu_write(session->io, SEND_PDU(session));
}

/**
  Decode an ERROR PDU into $err_code and $str.

  The length of $str will be placed in $len.

  On success, returns zero.  On failure, returns non-zero.
 */
int pdu_decode_ERROR(struct pdu *pdu, uint16_t *err_code, uint8_t **str, size_t *len)
{
	assert(pdu); // LCOV_EXCL_LINE
	assert(pdu->op == PROTOCOL_OP_ERROR); // LCOV_EXCL_LINE
	assert(err_code); // LCOV_EXCL_LINE
	assert(str); // LCOV_EXCL_LINE
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

	DEBUG("RECV ERROR (op:%u) - %u %s", PROTOCOL_OP_ERROR, *err_code, *str);
	return 0;
}

/**
  Send a FACTS PDU containing $facts to the remote end.

  This type of PDU is sent from the client to the server, sending along
  a hash of the local facts in order to receive a generated policy.

  On success, returns zero.  On failure, returns non-zero.
 */
int pdu_send_FACTS(struct session *session, const struct hash *facts)
{
	assert(session); // LCOV_EXCL_LINE
	assert(facts); // LCOV_EXCL_LINE

	struct pdu *pdu = SEND_PDU(session);
	struct stringlist *list = stringlist_new(NULL);
	struct hash_cursor cur;

	char *key, *value, kvp[256];

	char *buf;
	size_t len;

	for_each_key_value(facts, &cur, key, value) {
		snprintf(kvp, 256, "%s=%s\n", key, value);
		kvp[255] = '\0';
		stringlist_add(list, kvp); /* stringlist_add strdup's */
	}

	buf = stringlist_join(list, "");
	DEBUG("SEND FACTS (op:%u) - %u facts", pdu->op, list->num);
	stringlist_free(list);
	len = strlen(buf);

	if (pdu_allocate(pdu, PROTOCOL_OP_FACTS, len) < 0) {
		return -1;
	}

	memcpy(pdu->data, buf, len);
	free(buf);

	return pdu_write(session->io, SEND_PDU(session));
}

/**
  Decode a FACTS PDU into $facts.

  On success, returns zero.  On failure, returns non-zero.
 */
int pdu_decode_FACTS(struct pdu *pdu, struct hash *facts)
{
	assert(pdu); // LCOV_EXCL_LINE
	assert(pdu->op == PROTOCOL_OP_FACTS); // LCOV_EXCL_LINE
	assert(facts); // LCOV_EXCL_LINE

	size_t i;

	struct stringlist *lines = stringlist_split((char*)pdu->data, pdu->len, "\n", SPLIT_GREEDY);
	if (!lines) {
		return -1;
	}

	for (i = 0; i < lines->num; i++) {
		fact_parse(lines->strings[i], facts);
	}

	stringlist_free(lines);

	DEBUG("RECV FACTS (op:%u) - %u facts", pdu->op, lines->num);
	return 0;
}

/**
  Send a POLICY PDU, containing $policy, to the remote end.

  This type of PDU is sent from the server to a client, and includes the
  policy generated from the client facts given in the FACTS PDU.

  On success, returns zero.  On failure, returns non-zero.
 */
int pdu_send_POLICY(struct session *session, const struct policy *policy)
{
	assert(session); // LCOV_EXCL_LINE
	assert(policy); // LCOV_EXCL_LINE

	struct pdu *pdu = SEND_PDU(session);
	char *packed;
	size_t len;

	packed = policy_pack(policy);
	if (!packed) {
		return -1;
	}

	len = strlen(packed);

	if (pdu_allocate(pdu, PROTOCOL_OP_POLICY, len) < 0) {
		free(packed);
		return -1;
	}

	memcpy(pdu->data, packed, len);
	free(packed);

	DEBUG("SEND POLICY (op:%u) - (policy data)", pdu->op);
	return pdu_write(session->io, pdu);
}

int pdu_decode_POLICY(struct pdu *pdu, struct policy **policy)
{
	assert(pdu); // LCOV_EXCL_LINE
	assert(pdu->op == PROTOCOL_OP_POLICY); // LCOV_EXCL_LINE
	assert(policy); // LCOV_EXCL_LINE

	*policy = policy_unpack((char*)pdu->data);
	if (*policy) {
		return 0;
	} else {
		return -1;
	}
}

/**
  Send a FILE PDU to the remote end.

  FILE PDUs are sent by clients to server in order to request the contents
  of a res_file that need to be refreshed on the client machine.  For security
  reasons, only the $checksum is sent, to prevent "leeching" of files.

  On success, returns 0.  On failure, returns non-zero.
 */
int pdu_send_FILE(struct session *session, struct SHA1 *checksum)
{
	assert(session); // LCOV_EXCL_LINE
	assert(checksum); // LCOV_EXCL_LINE

	struct pdu *pdu = SEND_PDU(session);

	if (pdu_allocate(pdu, PROTOCOL_OP_FILE, SHA1_HEXLEN-1) < 0) {
		return -1;
	}

	memcpy(pdu->data, checksum->hex, pdu->len);

	return pdu_write(session->io, pdu);
}

/**
  Send a DATA PDU to the remote end.

  DATA PDUs are sent by servers to clients, in response to a FILE PDU.
  Each DATA PDU represents a chunk of the file, sent in order.  The final
  (EOF) DATA PDU is sent with a zero length.

  If $srcfd is a valid file descriptor (non-negative), each call to
  pdu_send_DATA will read data out of $srcfd until EOF is reached.

  Otherwise, data is read from the $data argument.

  On success, returns the number of bytes sent.
  On failure, returns a negative value.
 */
int pdu_send_DATA(struct session *session, int srcfd, const char *data)
{
	assert(session); // LCOV_EXCL_LINE
	assert(srcfd >= 0 || data); // LCOV_EXCL_LINE

	struct pdu *pdu = SEND_PDU(session);

	char chunk[FILE_DATA_BLOCK_SIZE] = {0};
	size_t len = 0, nread = 0;
	int write_status;

	if (srcfd >= 0) {
		do {
			nread = read(srcfd, chunk + len, FILE_DATA_BLOCK_SIZE - len);
			len += nread;
		} while (nread != 0 && len < FILE_DATA_BLOCK_SIZE);
	} else {
		nread = strlen(data);
		len = (nread > FILE_DATA_BLOCK_SIZE
			     ? FILE_DATA_BLOCK_SIZE
			     : nread);
		memcpy(chunk, data, len);
	}

	if (pdu_allocate(pdu, PROTOCOL_OP_DATA, len) < 0) {
		return -1;
	}

	memcpy(pdu->data, chunk, len); /* note: len could be 0 */

	write_status = pdu_write(session->io, pdu);
	if (write_status != 0) {
		return write_status;
	}

	return len;
}

/**
  Send a GET_CERT PDU to the remote end.

  The X.509 certificate request $csr will be sent in the PDU,
  and GET_CERT asks the server to return a signed X.509 certificate,
  if it has one.

  This is a crucial part of the certificate auto-enrollment proces.

  On success, returns 0.  On failure, returns non-zero.
 */
int pdu_send_GET_CERT(struct session *session, X509_REQ *csr)
{
	assert(session); // LCOV_EXCL_LINE

	struct pdu *pdu = SEND_PDU(session);
	BIO *bio;
	char *raw_csr = NULL;
	size_t len;
	int write_status;

	if (csr) {
		bio = BIO_new(BIO_s_mem());
		if (!bio) {
			return -1;
		}

		if (!PEM_write_bio_X509_REQ(bio, csr)) {
			BIO_free(bio);
			return -1;
		}

		len = (size_t)BIO_get_mem_data(bio, &raw_csr);
		if (pdu_allocate(pdu, PROTOCOL_OP_GET_CERT, len) < 0) {
			BIO_free(bio);
			return -1;
		}

		memcpy(pdu->data, raw_csr, len);
		BIO_free(bio);
	} else {
		if (pdu_allocate(pdu, PROTOCOL_OP_GET_CERT, 0) < 0) {
			return -1;
		}
	}

	write_status = pdu_write(session->io, pdu);
	if (write_status != 0) {
		return write_status;
	}

	return 0;
}

/**
  Decode a GET_CERT PDU into $csr.

  On success, returns.  On failure, returns non-zero.
 */
int pdu_decode_GET_CERT(struct pdu *pdu, X509_REQ **csr)
{
	assert(pdu); // LCOV_EXCL_LINE
	assert(pdu->op == PROTOCOL_OP_GET_CERT); // LCOV_EXCL_LINE
	assert(csr); // LCOV_EXCL_LINE

	BIO *bio;

	if (pdu->len == 0) {
		*csr = NULL;
		return 0;
	}

	bio = BIO_new(BIO_s_mem());
	if (!bio) {
		return -1;
	}

	BIO_write(bio, pdu->data, pdu->len);
	if (!PEM_read_bio_X509_REQ(bio, csr, NULL, NULL)) {
		BIO_free(bio);
		return -1;
	}

	BIO_free(bio);
	return 0;
}

/**
  Send a SEND_CERT PDU to the remote end.

  SEND_CERT PDUs are sent in response to a GET_CERT, if the policy
  master has a signed certificate on file for the client.  The X.509
  certificate should be passed in as $cert.

  On success, returns 0.  On failure, returns non-zero.
 */
int pdu_send_SEND_CERT(struct session *session, X509 *cert)
{
	assert(session); // LCOV_EXCL_LINE

	struct pdu *pdu = SEND_PDU(session);
	BIO *bio;
	char *raw_cert = NULL;
	size_t len;
	int write_status;

	if (cert) {
		bio = BIO_new(BIO_s_mem());
		if (!bio) {
			return -1;
		}

		if (!PEM_write_bio_X509(bio, cert)) {
			BIO_free(bio);
			return -1;
		}

		len = (size_t)BIO_get_mem_data(bio, &raw_cert);

		if (pdu_allocate(pdu, PROTOCOL_OP_SEND_CERT, len) < 0) {
			BIO_free(bio);
			return -1;
		}

		memcpy(pdu->data, raw_cert, len);
		BIO_free(bio);
	} else {
		len = 0;

		if (pdu_allocate(pdu, PROTOCOL_OP_SEND_CERT, 0) < 0) {
			return -1;
		}
	}

	write_status = pdu_write(session->io, pdu);
	if (write_status != 0) {
		return write_status;
	}

	return len;
}

/**
  Decode a SEND_CERT PDU into $cert.

  On succes, returns 0.  On failure, returns non-zero.
 */
int pdu_decode_SEND_CERT(struct pdu *pdu, X509 **cert)
{
	assert(pdu); // LCOV_EXCL_LINE
	assert(pdu->op == PROTOCOL_OP_SEND_CERT); // LCOV_EXCL_LINE
	assert(cert); // LCOV_EXCL_LINE

	BIO *bio;

	if (pdu->len > 0) {
		bio = BIO_new(BIO_s_mem());
		if (!bio) {
			return -1;
		}

		BIO_write(bio, pdu->data, pdu->len);
		if (!PEM_read_bio_X509(bio, cert, NULL, NULL)) {
			BIO_free(bio);
			return -1;
		}

		BIO_free(bio);
	} else {
		*cert = NULL;
	}
	return 0;
}

/**
  Send a REPORT PDU to the remote end, for $job.

  REPORT PDUs are sent to the policy master by clients after they
  have evaluated and enforced their policy locally.  The PDU contains
  a packed job structure that represents the run and everything that
  was attempted to bring the local system into compliance.

  On success, returns 0.  On failure, returns non-zero.
 */
int pdu_send_REPORT(struct session *session, struct job *job)
{
	assert(session); // LCOV_EXCL_LINE
	assert(job); // LCOV_EXCL_LINE

	struct pdu *pdu = SEND_PDU(session);
	char *packed;
	size_t len;

	packed = job_pack(job);
	if (!packed) {
		return -1;
	}

	len = strlen(packed);

	if (pdu_allocate(pdu, PROTOCOL_OP_REPORT, len) < 0) {
		free(packed);
		return -1;
	}

	memcpy(pdu->data, packed, len);
	free(packed);

	DEBUG("SEND REPORT (op:%u) - (report data)", pdu->op);
	return pdu_write(session->io, pdu);
}

/**
  Decode a REPORT PDU into $job.

  On success, returns 0.  On failure, returns non-zero.
 */
int pdu_decode_REPORT(struct pdu *pdu, struct job **job)
{
	assert(pdu); // LCOV_EXCL_LINE
	assert(pdu->op == PROTOCOL_OP_REPORT); // LCOV_EXCL_LINE
	assert(job); // LCOV_EXCL_LINE

	*job = job_unpack((char*)pdu->data);
	if (*job) {
		return 0;
	} else {
		return -1;
	}
}

/**
  Read $pdu from $io.

  On success, returns 0.  On failure, returns non-zero,
  and the contents of $pdu is undefined.
 */
int pdu_read(SSL *io, struct pdu *pdu)
{
	assert(io); // LCOV_EXCL_LINE
	assert(pdu); // LCOV_EXCL_LINE

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

	DEBUG("pdu_read:  OP:%04u/%s; LEN:%04u", pdu->op, protocol_op_names[pdu->op], pdu->len);
	pdu_dump(pdu);
	return 0;
}

/**
  Write $pdu to $io.

  On success, returns 0.  On failure, returns non-zero.
 */
int pdu_write(SSL *io, struct pdu *pdu)
{
	assert(io); // LCOV_EXCL_LINE
	assert(pdu); // LCOV_EXCL_LINE

	uint16_t op, len;
	int nwritten;

	op  = htons(pdu->op);
	len = htons(pdu->len);

	nwritten = SSL_write(io, &op, sizeof(op));
	if (nwritten != sizeof(op)) {
		DEBUG("pdu_write: error writing header:op");
		return -1; /* error writing header */
	}

	nwritten = SSL_write(io, &len, sizeof(len));
	if (nwritten != sizeof(len)) {
		DEBUG("pdu_write: error writing header:len");
		return -1; /* error writing header */
	}

	if (pdu->len > 0) {
		nwritten = SSL_write(io, pdu->data, pdu->len);
		if (nwritten != pdu->len) {
			DEBUG("pdu_write: error writing payload");
			return -3; /* error writing payload */
		}
	}

	DEBUG("pdu_write: OP:%04u/%s; LEN:%04u", pdu->op, protocol_op_names[pdu->op], pdu->len);
	pdu_dump(pdu);
	return 0;
}

/**
  Wait for the next PDU from the remote end.

  $session is the current protocol session.  This call
  will block until a PDU is available to be read.

  On success, returns 0.  On failure, returns non-zero.
  Failure indicates application-level issues (the ERROR PDU).
 */
int pdu_receive(struct session *session)
{
	assert(session); // LCOV_EXCL_LINE

	struct pdu *pdu = RECV_PDU(session);
	int rc;

	rc = pdu_read(session->io, pdu);
	if (rc < 0) {
		DEBUG("pdu_receive: pdu_read returned %i", rc);
		return -1;
	}

	/* handle error response */
	if (pdu->op == PROTOCOL_OP_ERROR) {
		free(session->errstr);
		if (pdu_decode_ERROR(pdu, &(session->errnum), &(session->errstr), NULL) < 0) {
			perror("Unable to decode ERROR PDU");
			return -1;
		}

		DEBUG(" -> ERROR (op:%u) - %u %s", PROTOCOL_OP_ERROR, session->errnum, session->errstr);
		return -1;
	}

	return 0;
}

/**********************************************************/

static int ssl_loaded = 0;
/** Initialize the OpenSSL library */
void protocol_ssl_init(void)
{
	if (ssl_loaded == 1) { return; }
	ssl_loaded = 1;

	if (!SSL_library_init()) {
		CRITICAL("protocol_ssl_init: Failed to initialize OpenSSL");
		exit(1);
	}
	SSL_load_error_strings();
	RAND_load_file("/dev/urandom", 1024);
}

/** Get the name of $op.  */
const char* protocol_op_name(enum proto_op op)
{
	if (op > protocol_op_max) { return NULL; }
	return protocol_op_names[op];
}

/**
  Initialize a $session, using $io for the IO stream.

  **Note:** this function does *not* allocate a new session
  structure.  Nevertheless, teh caller must match this call with one
  to @protocol_session_deinit, to free memory allocated by this function.

  On success, returns 0.  On failure, returns non-zero.
 */
int protocol_session_init(struct session *session, SSL *io)
{
	session->io = io;

	memset(SEND_PDU(session), 0, sizeof(struct pdu));
	memset(RECV_PDU(session), 0, sizeof(struct pdu));

	session->errnum = 0;
	session->errstr = NULL;

	return 0;
}

/**
  De-initialize a $session

  **Note:** since @protocol_session_init does not allocate the session
  structure itself, this function does not free the pointer passed to it.
  If the caller allocates the structure on the heap, the caller is responsible
  for freeing that memory.

  On success, returns 0.  On failure, returns non-zero.
 */
int protocol_session_deinit(struct session *session)
{
	session->io = NULL;

	pdu_deallocate(SEND_PDU(session));
	memset(SEND_PDU(session), 0, sizeof(struct pdu));

	pdu_deallocate(RECV_PDU(session));
	memset(RECV_PDU(session), 0, sizeof(struct pdu));

	session->errnum = 0;
	free(session->errstr);

	return 0;
}

/**
  Verify peer certificate by checking $hostname.

  This is a callback, used in the OpenSSL calls, to verify
  the validity of the peer certificate by comparing the
  subjectAltName/DNS certificate extension against $hostname.

  How $hostname is determined is left to the discretion of the
  caller.  For security's sake, some sort of reverse DNS lookup
  against the peer address is recommended.

  See @protocol_reverse_lookup_verify

  If verification succeeds, returns 0.
  On failure, returns an OpenSSL error code.
 */
long protocol_ssl_verify_peer(SSL *ssl, const char *host)
{
	X509 *cert;
	X509_NAME *subj;
	char data[256];
	int extcount;
	int ok = 0;

	if (!(cert = SSL_get_peer_certificate(ssl)) || !host) {
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
				const X509V3_EXT_METHOD *meth;

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
			ERROR("Peer certificate FQDN did not match FCrDNS lookup");
			goto err_occurred;
		}
	}

	X509_free(cert);
	return SSL_get_verify_result(ssl);

err_occurred:
	X509_free(cert);
	return X509_V_ERR_APPLICATION_VERIFICATION;
}

/** Log SSL errors via @DEBUG.  */
void protocol_ssl_backtrace(void)
{
	unsigned long e;

	while ( (e = ERR_get_error()) != 0 ) {
		DEBUG("SSL(%u): %s", e, ERR_reason_error_string(e));
	}
}

/**
  Reverse DNS lookup.

  This function is used during the peer certificate verification
  process;  the FQDN of the remote host is looked up so that it
  can be compared to that stored in the certificate extensions
  (subjectAltName / DNS).

  The peer address is determined from $sockfd.  The resulting
  hostname will be stored in $buf.  At most, $len - 1 characters
  will be stored, and $buf will be NULL-terminated.

  See @protocol_ssl_verify_peer

  On success, returns 0.  On failure, returns non-zero.
 */
int protocol_reverse_lookup_verify(int sockfd, char *buf, size_t len)
{
	struct sockaddr_in ipv4;
	socklen_t ipv4_len;

	ipv4_len = sizeof(ipv4);
	memset(&ipv4, 0, ipv4_len);

	int rc;
	rc = getpeername(sockfd, (struct sockaddr*)(&ipv4), &ipv4_len);
	if (rc != 0) {
		DEBUG("getpeername failed for socket %d", sockfd);
		return -1;
	}

	char str[128];
	if (inet_ntop(AF_INET, &ipv4.sin_addr, str, sizeof(str)) != NULL) {
		DEBUG("getpeername returned %s", str);
	}

	rc = getnameinfo((struct sockaddr*)(&ipv4), ipv4_len, buf, len, NULL, 0, NI_NAMEREQD);
	if (rc != 0) {
		DEBUG("getnameinfo failed for socket %d", sockfd);
		DEBUG("gai_error: %s", gai_strerror(rc));
		return -2;
	}

	return 0;
}
