#ifndef _NET_H
#define _NET_H

#include <openssl/bio.h>

#define NETWORK_BUFFER_MAX 8192

typedef struct {

	BIO     *io; /* OpenSSL BIO object to read from / write to */

	char     recv[NETWORK_BUFFER_MAX]; /* Raw buffer for storing read data */
	char     send[NETWORK_BUFFER_MAX]; /* Buffer for writing data */

	char    *recv_pos; /* Position in recv of next character */
	size_t   recv_len; /* Amount of usable data in recv */

} network_buffer;

void network_buffer_init(network_buffer *buffer, BIO *io);
size_t network_write(network_buffer *buffer, char *src, size_t len);
size_t network_printf(network_buffer *buffer, char *fmt, ...);
size_t network_readline(network_buffer *buffer, char *str, size_t n);

#endif
