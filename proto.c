#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "proto.h"
#include "net.h"

static int __proto_abort(struct connection *conn);
static int __proto_error(struct connection *conn);
static int __proto_readline(struct connection *conn);

static int server_helo(struct connection *conn);
static int server_identify(struct connection *conn);
static int server_main(struct connection *conn);
static int server_recv_report(struct connection *conn);

/**********************************************************/

static int __proto_abort(struct connection *conn)
{
	net_writeline(&conn->nbuf, "500 Internal Error\r\n");
	close(conn->fd);
	exit(1);
}

static int __proto_error(struct connection *conn)
{
	net_writeline(&conn->nbuf, "601 Protocol Error\r\n");
	close(conn->fd);
	return -1;
}

static int __proto_readline(struct connection *conn)
{
	conn->len = net_readline(&conn->nbuf, conn->line, PROTO_LINE_MAX);
	if (conn->len < 0) {
		__proto_abort(conn);
	}
	return 0;
}

static int server_helo(struct connection *conn)
{
	if (net_writeline(&conn->nbuf, "HELO %s\r\n", conn->server) < 0) {
		perror("write failed");
		exit(1);
	}

	__proto_readline(conn);
	if (strncmp(conn->line, "HELO ", 5) == 0) {
		return server_identify(conn);
	}
	return __proto_error(conn);
}

static int server_identify(struct connection *conn)
{
	char *client = (char*)conn->line + 5;
	if (strcmp(client, "client.example.net") == 0) {
		net_writeline(&conn->nbuf, "202 Identity Known\r\n");
		conn->next = server_main;
		return 0;
	} else {
		net_writeline(&conn->nbuf, "401 Identity Unknown\r\n");
		close(conn->fd);
		return -1;
	}
}

static int server_main(struct connection *conn)
{
	__proto_readline(conn);
	if (strcmp(conn->line, "BYE") == 0) {
		close(conn->fd);
		exit(0);
	} else if (strcmp(conn->line, "QUERY") == 0) {
		net_writeline(&conn->nbuf, "200 OK\r\n");
		net_writeline(&conn->nbuf, "VERSION 453256\r\n");
		return 0;
	} else if (strcmp(conn->line, "RETRIEVE") == 0) {
		net_writeline(&conn->nbuf, "203 Sending Data\r\n");
		net_writeline(&conn->nbuf, "<data>\r\n");
		net_writeline(&conn->nbuf, "<data>\r\n");
		net_writeline(&conn->nbuf, "<data>\r\n");
		net_writeline(&conn->nbuf, "<data>\r\n");
		net_writeline(&conn->nbuf, "DONE\r\n");
		return 0;
	} else if (strcmp(conn->line, "REPORT") == 0) {
		net_writeline(&conn->nbuf, "301 Go Ahead\r\n");
		conn->next = server_recv_report;
		return 0;
	}
	return __proto_error(conn);
}

static int server_recv_report(struct connection *conn)
{
	__proto_readline(conn);
	while (strcmp(conn->line, "DONE") != 0) {
		__proto_readline(conn);
	}
	net_writeline(&conn->nbuf, "200 OK\r\n");
	conn->next = server_main;
	return 0;
}

/**********************************************************/

int proto_init(struct connection *conn, int fd, const char *name)
{
	conn->len = 0;
	conn->next = NULL;
	memset(conn->line, 0, PROTO_LINE_MAX);

	conn->server = strdup(name);
	conn->fd = fd;

	netbuf_init(&conn->nbuf, fd);

	return 0;
}

int server_dispatch(struct connection *conn)
{
	conn->next = server_helo;

	while ( conn->next && (*(conn->next))(conn) == 0 )
		;

	close(conn->fd);
	return 0;
}

#define STATUS_IS(s,n) ((s) >= n && (s) < n + 100)
static int __client_status(char *data)
{
	char code[4] = {0};
	strncpy(code, data, 3);
	return atoi(code);
}

/**
  client_helo - Open Hailing Frequencies

  This is the first step on the client side of the protocol
  exchange; the client must read the HELO sent by the server
  and respond in kind.  If challenged, the client will provide
  its identity to the server for review.

  client_helo returns 0 if the server responded with a
  202 Identity Known status.  In this case, no challenge-response
  takes place with respect to client identity.

  client_helo returns -1 if the server responded with a
  401 Identity Unknown status.  In this case, the identity is
  sent in response to the challenge.

 */
int client_helo(netbuf *buf, char *name, char *identity)
{
	char data[255];
	size_t len;
	int status;

	/* FIXME: discarding server HELO */
	net_readline(buf, data, 255); /* FIXME: handle failure */

	if (net_writeline(buf, "HELO %s\r\n", name) < 0) {
		perror("client_helo");
		return -1;
	}

	net_readline(buf, data, 255); /* FIXME: handle failure */
	switch (status = __client_status(data)) {
	case 202:
		return 0;
	case 401:
		break;
	default:
		return -2; /* protocol error */
	}

	if (net_writeline(buf, "IDENT %s %s\r\n", name, identity) < 0) {
		perror("client_helo");
		return -1;
	}

	net_readline(buf, data, len); /* FIXME: handle failure */
	return -1;
}

int client_query(netbuf *buf, long *version)
{
	char data[255];
	size_t len;
	int status;


	*version = 0;

	if (net_writeline(buf, "QUERY\r\n") < 0) {
		perror("client_query");
		return -1;
	}
	len = net_readline(buf, data, 255); /* FIXME: handle failure */

	status = __client_status(data);
	if (STATUS_IS(status, 200)) {
		len = net_readline(buf, data, 255);
		*version = atol(data + 8); /* skip "VERSION ", 8 bytes */
		return 200;
	} else {
		return status;
	}
}

int client_retrieve(netbuf *buf, char **data, size_t *n)
{
}

int client_report(netbuf *buf, char *data, size_t n)
{
}

int client_bye(netbuf *buf)
{
	net_writeline(buf, "BYE\r\n");
	close(buf->fd);
	return 0;
}

