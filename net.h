#ifndef _NET_H
#define _NET_H

#define NBUF_MAX 8192
typedef struct {
	int     fd;
	char    data[NBUF_MAX];
	char   *start;
	size_t  len;

	char    recv[NBUF_MAX];
	size_t  recv_len;
	char   *recv0;

	char    send[NBUF_MAX];
	size_t  send_len;
} netbuf;


void netbuf_init(netbuf *buf, int fd);
char netbuf_getc(netbuf *buf);

/* int net_read(netbuf *buf, *dst, size_t len); */
int net_write(netbuf *buf, char *src, size_t n);
int net_writeline(netbuf *buf, char *fmt, ...);
int net_readline(netbuf *buf, char *dst, size_t n);

int net_connect(const char *address, unsigned short port);

int net_listen(const char *address, unsigned short port);
int net_accept(int fd);

#endif
