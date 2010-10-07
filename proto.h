#ifndef _PROTO_H
#define _PROTO_H

#include <sys/types.h>
#include "net.h"

#define PROTO_LINE_MAX 255

struct connection;
typedef int (*handler)(struct connection*);
struct connection {
	char   *server; /* Name of server, for HELO */

	netbuf  nbuf;   /* Send / Receive Buffer, for line-oriented protocol */

	int     fd;
	char    line[PROTO_LINE_MAX];
	size_t  len;

	handler next;
};

int proto_init(struct connection *conn, int fd, const char *name);

int server_dispatch(struct connection *conn);

int client_helo(netbuf *buf, char *name, char *identity);
int client_query(netbuf *buf, long *version);
int client_retrieve(netbuf *buf, char **data, size_t *len);
int client_report(netbuf *buf, char *data, size_t len);
int client_bye(netbuf *buf);

#endif
