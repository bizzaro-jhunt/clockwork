#ifndef _PROTO_H
#define _PROTO_H

#include <sys/types.h>

struct connection;
typedef int (*handler)(struct connection*);

int server_dispatch(int connfd);

#endif
