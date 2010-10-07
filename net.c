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

static void netbuf_dump(netbuf *buf);
static int  netbuf_fill_recv(netbuf *buf);

/**************************************************************/

static void netbuf_dump(netbuf *buf)
{
	size_t i;
	fprintf(stderr, "--[ recv ]-----------------------------------------------------\n");
	for (i = 0; i < buf->recv_len; i++) {
		fprintf(stderr, "%c", buf->recv0[i]);
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "---------------------------------------------------------------\n");
	fprintf(stderr, "%u bytes total\n\n", buf->recv_len);

	fprintf(stderr, "--[ send ]-----------------------------------------------------\n");
	fprintf(stderr, "%s\n", buf->send);
	fprintf(stderr, "---------------------------------------------------------------\n");
	fprintf(stderr, "%u bytes total\n", buf->send_len);
}

static int netbuf_fill_recv(netbuf *buf)
{
	buf->recv0 = buf->recv;
	buf->recv_len = read(buf->fd, buf->recv, NBUF_MAX);
	return buf->recv_len;
}

/**************************************************************/

void netbuf_init(netbuf *buf, int fd)
{
	buf->fd = fd;

	buf->recv_len = 0;
	memset(buf->recv, 0, NBUF_MAX);
	buf->recv0 = buf->recv;

	memset(buf->send, 0, NBUF_MAX);
}

char netbuf_getc(netbuf *buf)
{
	int rc;

	if (buf->recv_len <= 0) {
		rc = netbuf_fill_recv(buf);
		if (rc < 0) { /* error reading */
			perror("netbuf_getc");
			exit(0); /* FIXME: handle this better */
		} else if (rc == 0) {
			return '\0';
		}
	}
	buf->recv_len--;
	return *buf->recv0++;
}

/**************************************************************/

int net_writeline(netbuf *buf, char *fmt, ...)
{
	va_list args;
	size_t len;

	va_start(args, fmt);
	len = vsnprintf(buf->send, NBUF_MAX, fmt, args);
	if (len < 0) {
		/* error in snprintf */
		return -1;
	} else if (len > NBUF_MAX) {
		/* data overflow */
		return -1;
	}

	fprintf(stderr, "send: (%u) %s", len, buf->send);
	return write(buf->fd, buf->send, len);
}

int net_readline(netbuf *buf, char *dst, size_t n)
{
	char *line = dst;
	size_t len = 0;
	char c;

	/* clear out CR/LF */
	for (c = netbuf_getc(buf); c != '\0' && CRLF(c); c = netbuf_getc(buf))
		;

	for (; len < n && c && !CRLF(c); c = netbuf_getc(buf)) {
		len++;
		*dst++ = c;
	}
	*dst = '\0';

	fprintf(stderr, "recv: (%u) %s\n", len, line);

	return len;
}

int net_connect(const char *address, uint16_t port)
{
	int fd;
	struct sockaddr_in sa;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) { return -1; }

	/* set up sa with address / port to bind to */
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port); /* in_port_t == uint16_t */

	errno = 0;
	if (inet_pton(AF_INET, address, &sa.sin_addr) != 1) {
		if (errno == 0) { errno = EINVAL; }
		return -1;
	}

	if (connect(fd, SA(&sa), sizeof(sa)) != 0) {
		return -1;
	}

	return fd;
}


int net_listen(const char *address, uint16_t port)
{
	int fd;
	struct sockaddr_in sa;

	/* get a socket from the kernel */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) { return -1; }

	/* set up sa with address / port to listen on */
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port); /* in_port_t == uint16_t */

	errno = 0;
	if (inet_pton(AF_INET, address, &sa.sin_addr) != 1) {
		if (errno == 0) { errno = EINVAL; }
		return -1;
	}

	/* bind the socket to a listening address */
	if (bind(fd, SA(&sa), sizeof(sa)) != 0) {
		return -1;
	}

	/* listen on the bound socket */
	if (listen(fd, 0) != 0) {
		return -1;
	}

	return fd;
}

int net_accept(int fd)
{
	int conn;

	conn = accept(fd, NULL, NULL);
	if (conn < 0) { return -1; }

	switch (fork()) {
		case -1: return -1;
		case  0: /* in child */
			close(fd);
			return conn;
		default: /* in parent */
			close(conn);
			return 0;
	}
}

