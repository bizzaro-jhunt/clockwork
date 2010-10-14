#include <assert.h>
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

#define SA(s) ((struct sockaddr *)(s))

#define CRLF(c) ((c) == '\n' || (c) == '\r')

static char network_getc(network_buffer *buffer)
{
	int nread;

	if (buffer->recv_len <= 0) {
		buffer->recv_pos = buffer->recv;
again:
		nread = BIO_read(buffer->io, buffer->recv, sizeof(buffer->recv));
		if (nread <= 0) {
			fprintf(stderr, "network_getc: got %i back from BIO_read\n", nread);
			if (BIO_should_retry(buffer->io)) {
				fprintf(stderr, "network_getc: should retry\n");
				goto again;
			} else {
				return '\0'; /* error condition */
			}
		}
		buffer->recv_len = nread;
	}
	buffer->recv_len--;
	return *buffer->recv_pos++;
}

/**************************************************************/

void network_buffer_init(network_buffer *buffer, BIO *io)
{
	assert(buffer);
	assert(io);

	buffer->io = io;
	buffer->recv_len = 0;
	memset(buffer->recv, 0, sizeof(buffer->recv));
	memset(buffer->send, 0, sizeof(buffer->send));
	buffer->recv_pos = buffer->recv;
}

size_t network_write(network_buffer *buffer, char *src, size_t len)
{
	return BIO_write(buffer->io, src, len);
}

size_t network_printf(network_buffer *buffer, char *fmt, ...)
{
	va_list args;
	size_t len;

	va_start(args, fmt);
	len = vsnprintf(buffer->send, NETWORK_BUFFER_MAX, fmt, args);
	if (len < 0) {
		/* error in vsnprintf */
		return -1;
	} else if (len > NETWORK_BUFFER_MAX) {
		/* data overflow */
		return -1;
	}
	return network_write(buffer, buffer->send, len);
}

size_t network_readline(network_buffer *buffer, char *str, size_t n)
{
	char *line = str;
	size_t len = 0;
	char c;

	/* Eat up newline characters from previous runs */
	for (c = network_getc(buffer); c != '\0' && CRLF(c); c = network_getc(buffer))
		;

	for (; len < n && c && !CRLF(c); c = network_getc(buffer)) {
		len++;
		*str++ = c;
	}
	*str = '\0';

	fprintf(stderr, "recv: (%u) %s\n", len, line);
	return len;
}

