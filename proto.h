#ifndef _PROTO_H
#define _PROTO_H

#include <sys/types.h>
#include "net.h"

#define PROTO_LINE_MAX 256 

struct connection;
typedef int (*handler)(struct connection*);
struct connection {
	char   *server; /* Name of server, for HELO */
	char   *name[2];  /* name of local side (0) and remote side (1) */
	char   *ident[2]; /* IDENT of local side (0) and remote side (0) */

	long    version;  /* Version sent / to send */

	netbuf  nbuf;   /* Send / Receive Buffer, for line-oriented protocol */

	int     fd;
	char    line[PROTO_LINE_MAX];
	size_t  len;

	handler next;
};

int proto_init(struct connection *conn, int fd, const char *name, const char *ident);

int server_dispatch(struct connection *conn);

int client_helo(struct connection *conn);
int client_query(struct connection *conn);
int client_retrieve(struct connection *conn, char **data, size_t *len);
int client_report(struct connection *conn, char *data, size_t len);
int client_bye(struct connection *conn);

#endif
