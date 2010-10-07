#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <poll.h>

#include "proto.h"

#define NBUF_MAX 8192
typedef struct {
	int     fd;
	char    data[NBUF_MAX];
	char   *start;
	size_t  len;

	struct  pollfds[2]; /* read / write fd pollers */

} netbuf;

static void netbuf_init(netbuf *buf, int fd);
static int netbuf_fill(netbuf *buf);
static char netbuf_getc(netbuf *buf);

static void netbuf_init(netbuf *buf, int fd)
{
	buf->fd = fd;
	buf->len = 0;
	memset(buf->data, 0, 8192);
	buf->start = buf->data;
}

static void netbuf_dump(netbuf *buf)
{
	size_t i;
	printf("---------------------------------------------------------------\n");
	for (i = 0; i < buf->len; i++) {
		printf("%c", buf->start[i]);
	}
	printf("\n");
	printf("---------------------------------------------------------------\n");
	printf("%u bytes total\n", buf->len);
}

static int netbuf_fill(netbuf *buf)
{
	fprintf(stderr, "in netbuf_fill\n");
	buf->start = buf->data;
	buf->len = read(buf->fd, buf->data, NBUF_MAX);
	netbuf_dump(buf);
	return buf->len;
}

static char netbuf_getc(netbuf *buf)
{
	int rc;

	if (buf->len <= 0) {
		rc = netbuf_fill(buf);
		if (rc < 0) { /* error reading */
			perror("netbuf_getc");
			exit(0); /* FIXME: handle this better */
		} else if (rc == 0) {
			printf("got 0 from netbuf_fill\n");
			return '\0';
		}
	}
	buf->len--;

	fprintf(stderr, "netbuf_getc: fetching character '%c' (0x%02x); buf->len is %u\n", *buf->start, *buf->start, buf->len);
	return *buf->start++;
}

static int net_readline(netbuf *buf, char *dst, size_t n);

#define CRLF(c) ((c) == '\n' || (c) == '\r')
static int net_readline(netbuf *buf, char *dst, size_t n)
{
	size_t len = 0;
	char c;

	fprintf(stderr, "netbuf_readline: called\n");
	/* clear out CR/LF */
	for (c = netbuf_getc(buf); c != '\0' && CRLF(c); c = netbuf_getc(buf))
		;

	for (; len < n && c && !CRLF(c); c = netbuf_getc(buf)) {
		len++;
		*dst++ = c;
	}
	*dst = '\0';

	return len;
}

/**********************************************************/
/**********************************************************/

#define PROTO_LINE_MAX 255
struct connection {
	char   *server;    /* Name of server, for initial HELO */

	netbuf  recv_buf;  /* Receive Buffer, for line-oriented protocol */

	int     fd;
	char    line[PROTO_LINE_MAX];
	size_t  len;

	handler next;
};

#define __proto_status(fd,code,status) \
	write(fd, #code " " status "\n", strlen(#code " " status "\n"));
static int __proto_abort(struct connection *conn);
static int __proto_error(struct connection *conn);
static int __proto_readline(struct connection *conn);

int server_proto_helo(struct connection *conn);
int server_proto_identify(struct connection *conn);
int server_proto_main(struct connection *conn);
int server_proto_recv_report(struct connection *conn);

/**********************************************************/

int __proto_abort(struct connection *conn)
{
	__proto_status(conn->fd, 500, "Internal Error");
	close(conn->fd);
	exit(1);
}

int __proto_error(struct connection *conn)
{
	__proto_status(conn->fd, 601, "Protocol Error");
	close(conn->fd);
	return -1;
}

int __proto_readline(struct connection *conn)
{
	conn->len = net_readline(&conn->recv_buf, &conn->line[0], PROTO_LINE_MAX);
	if (conn->len < 0) {
		__proto_abort(conn);
	}

	fprintf(stderr, "+ (%u) %s\n", conn->len, conn->line);
	return 0;
}

int server_proto_helo(struct connection *conn)
{
	conn->len = snprintf(conn->line, PROTO_LINE_MAX, "HELO %s\r\n", conn->server);
	if (write(conn->fd, conn->line, conn->len) != conn->len) {
		perror("write failed");
		exit(1);
	}

	__proto_readline(conn);
	fprintf(stderr, "server_proto_helo: Checking HELO response from client\n");
	if (strncmp(conn->line, "HELO ", 5) == 0) {
		fprintf(stderr, "server_proto_helo: found a HELO from the client\n");
		return server_proto_identify(conn);
	}
	fprintf(stderr, "server_proto_helo: no HELO found.  invoking 601\n");
	return __proto_error(conn);
}

int server_proto_identify(struct connection *conn)
{
	char *client = (char*)conn->line + 5;
	fprintf(stderr, "server_proto_identify: checking client '%s'\n", client);
	if (strcmp(client, "client.example.net") == 0) {
		fprintf(stderr, "server_proto_identify: known client; moving on to server_proto_main\n");
		__proto_status(conn->fd, 202, "Identity Known");
		fprintf(stderr, "server_proto_identify: sent 202 status\n");
		conn->next = server_proto_main;
		return 0;
	} else {
		fprintf(stderr, "server_proto_identify: unknown client; invoking 401\n");
		__proto_status(conn->fd, 401, "Identity Unknown");
		fprintf(stderr, "server_proto_identify: sent 401 status\n");
		close(conn->fd);
		return -1;
	}
}

int server_proto_main(struct connection *conn)
{
	__proto_readline(conn);
	if (strcmp(conn->line, "BYE") == 0) {
		printf("Saying goodbye\n");
		close(conn->fd);
		exit(0);
	} else if (strcmp(conn->line, "QUERY") == 0) {
		__proto_status(conn->fd, 200, "OK");
		write(conn->fd, "VERSION 4.32.6\n", strlen("VERSION 4.32.6\n"));
		return 0;
	} else if (strcmp(conn->line, "RETRIEVE") == 0) {
		__proto_status(conn->fd, 203, "Sending Data");
		write(conn->fd, "<data>\n", 7);
		write(conn->fd, "<data>\n", 7);
		write(conn->fd, "<data>\n", 7);
		write(conn->fd, "<data>\n", 7);
		write(conn->fd, "DONE\n", 5);
		return 0;
	} else if (strcmp(conn->line, "REPORT") == 0) {
		__proto_status(conn->fd, 301, "Go Ahead");
		conn->next = server_proto_recv_report;
		return 0;
	}
	return __proto_error(conn);
}

int server_proto_recv_report(struct connection *conn)
{
	__proto_readline(conn);
	while (strcmp(conn->line, "DONE") != 0) {
		printf(" >> recv_report: %s\n", conn->line);
		__proto_readline(conn);
	}
	__proto_status(conn->fd, 200, "OK");
	conn->next = server_proto_main;
	return 0;
}

/**********************************************************/

int server_dispatch(int connfd)
{
	struct connection conn = {
		.server = "server.example.com",
		.line = {0},
		.len = 0,
		.fd = connfd,
		.next = server_proto_helo
	};
	netbuf_init(&conn.recv_buf, connfd);

	while ( conn.next && (*(conn.next))(&conn) == 0 )
		;

	close(conn.fd);
	return 0;
}

