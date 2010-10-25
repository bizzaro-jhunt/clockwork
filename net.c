#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "net.h"

int verify_callback(int ok, X509_STORE_CTX *store)
{
	char data[256];

	if (!ok) {
		X509 *cert = X509_STORE_CTX_get_current_cert(store);
		int depth  = X509_STORE_CTX_get_error_depth(store);
		int err    = X509_STORE_CTX_get_error(store);

		fprintf(stderr, "-Error with certificate at depth: %i\n", depth);
		X509_NAME_oneline(X509_get_issuer_name(cert), data, 256);
		fprintf(stderr, "  issuer   = %s\n", data);
		X509_NAME_oneline(X509_get_subject_name(cert), data, 256);
		fprintf(stderr, "  subject  = %s\n", data);
		fprintf(stderr, "  err %i:%s\n", err, X509_verify_cert_error_string(err));
	}

	return ok;
}

long post_connection_check(SSL *ssl, char *host)
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
			const unsigned char *extstr;
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
	if (cert) {
		X509_free(cert);
	}
	return X509_V_ERR_APPLICATION_VERIFICATION;
}
