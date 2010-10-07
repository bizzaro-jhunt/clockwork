#ifndef _NET_H
#define _NET_H

int net_listen(const char *address, unsigned short port);
int net_accept(int fd);

#endif
