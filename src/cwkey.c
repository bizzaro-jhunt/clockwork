/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>

#include "clockwork.h"

static char* s_myfqdn(void)
{
	char nodename[1024];
	nodename[1023] = '\0';
	gethostname(nodename, 1023);

	struct addrinfo hints, *info, *p;
	memset(&hints, 0, sizeof hints);
	hints.ai_family   = AF_UNSPEC; /*either IPV4 or IPV6*/
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_CANONNAME;

	int rc = getaddrinfo(nodename, NULL, &hints, &info);
	if (rc != 0) {
		cw_log(LOG_ERR, "Failed to lookup %s: %s", nodename, gai_strerror(rc));
		return NULL;
	}

	char *ret = NULL;
	for (p = info; p != NULL; p = p->ai_next) {
		if (strcmp(p->ai_canonname, nodename) == 0) continue;
		ret = strdup(p->ai_canonname);
		break;
	}

	freeaddrinfo(info);
	return ret ? ret : strdup(nodename);
}

int main(int argc, char **argv)
{
	const char *short_opts = "h?f:i:";
	struct option long_opts[] = {
		{ "help",     no_argument,       NULL, 'h' },
		{ "identity", required_argument, NULL, 'i' },
		{ "file",     required_argument, NULL, 'f' },
		{ 0, 0, 0, 0 },
	};

	char *ident = NULL;
	char *file  = strdup("cwcert");

	int opt, idx = 0;
	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			printf("USAGE: %s [--identity FQDN] [--file cwcert]\n", argv[0]);
			exit(0);

		case 'i':
			free(ident);
			ident = strdup(optarg);
			break;

		case 'f':
			free(file);
			file = strdup(optarg);
			break;
		}
	}

	char *pubfile = cw_string("%s.pub", file);
	int pubfd = open(pubfile, O_WRONLY|O_CREAT|O_EXCL, 0444);
	if (pubfd < 0) {
		perror(pubfile);
		exit(1);
	}
	int secfd = open(file, O_WRONLY|O_CREAT|O_EXCL, 0400);
	if (secfd < 0) {
		perror(file);
		unlink(pubfile);
		exit(1);
	}

	FILE *pubio = fdopen(pubfd, "w");
	FILE *secio = fdopen(secfd, "w");
	if (!pubio || !secio) {
		perror("fdopen");
		fclose(pubio); close(pubfd); unlink(pubfile);
		fclose(secio); close(secfd); unlink(file);
		exit(1);
	}

	cw_cert_t *cert = cw_cert_generate();
	assert(cert);
	cert->ident = ident ? ident : s_myfqdn();

	cw_cert_writeio(cert, pubio, 0);
	cw_cert_writeio(cert, secio, 1);

	fclose(pubio);
	fclose(secio);

	cw_cert_destroy(cert);
	return 0;
}
