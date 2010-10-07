#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "proto.h"
#include "net.h"

static int __proto_abort(struct connection *conn);
static int __proto_error(struct connection *conn);
static int __proto_readline(struct connection *conn);

static int server_helo(struct connection *conn);
static int server_main(struct connection *conn);

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
	if (net_writeline(&conn->nbuf, "HELO %s\r\n", conn->name[0]) < 0) {
		perror("write failed");
		exit(1);
	}

	__proto_readline(conn);
	if (strncmp(conn->line, "HELO ", 5) == 0) {
		conn->name[1] = strdup(conn->line + 5);
		if (strcmp(conn->name[1], "client.example.net") == 0) {
			net_writeline(&conn->nbuf, "202 Identity Known\r\n");
			conn->next = server_main;
			return 0;
		} else {
			net_writeline(&conn->nbuf, "401 Identity Unknown\r\n");
			close(conn->fd);
			return -1;
		}
	}
	return __proto_error(conn);
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
		__proto_readline(conn);
		while (strcmp(conn->line, "DONE") != 0) {
			__proto_readline(conn);
		}
		net_writeline(&conn->nbuf, "200 OK\r\n");
		conn->next = server_main;
		return 0;
	}
	return __proto_error(conn);
}

/**********************************************************/

int proto_init(struct connection *conn, int fd, const char *name, const char *ident)
{
	conn->len = 0;
	conn->next = NULL;
	memset(conn->line, 0, PROTO_LINE_MAX);

	conn->name[0] = strdup(name);
	conn->ident[0] = strdup(ident);
	conn->fd = fd;

	conn->version = -1;

	netbuf_init(&conn->nbuf, fd);

	return 0;
}

int server_dispatch(struct connection *conn)
{

	fprintf(stderr, "  server process started\n");
	fprintf(stderr, "  %s : %s\n", conn->name[0], conn->ident[0]);

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
int client_helo(struct connection *conn)
{
	char data[255];
	size_t len;
	int status;

	__proto_readline(conn);
	conn->name[1] = strdup(conn->line);;

	if (net_writeline(&conn->nbuf, "HELO %s\r\n", conn->name[0]) < 0) {
		perror("client_helo");
		return -1;
	}

	__proto_readline(conn);
	switch (status = __client_status(conn->line)) {
	case 202:
		return 0;
	case 401:
		break;
	default:
		return -2; /* protocol error */
	}

	if (net_writeline(&conn->nbuf, "IDENT %s %s\r\n", conn->name[0], conn->ident[0]) < 0) {
		perror("client_helo");
		return -1;
	}

	__proto_readline(conn);
	/* FIXME: how to handle 201 Identity Accepted */
	return -1;
}

int client_query(struct connection *conn)
{
	char data[255];
	size_t len;
	int status;

	if (net_writeline(&conn->nbuf, "QUERY\r\n") < 0) {
		perror("client_query");
		return -1;
	}
	__proto_readline(conn);
	status = __client_status(conn->line);

	if (STATUS_IS(status, 200)) {
		__proto_readline(conn);
		conn->version = atol(conn->line + 8); /* skip "VERSION ", 8 bytes */
		return 200;
	} else {
		return status;
	}
}

int client_retrieve(struct connection *conn, char **data, size_t *n)
{
}

int client_report(struct connection *conn, char *data, size_t n)
{
}

int client_bye(struct connection *conn)
{
	net_writeline(&conn->nbuf, "BYE\r\n");
	close(conn->fd);
	return 0;
}

