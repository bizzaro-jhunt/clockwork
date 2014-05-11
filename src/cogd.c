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

#include "clockwork.h"
#include "pendulum.h"

#include <zmq.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netdb.h>

#include "policy.h"

#define INTERVAL 1000 * 5

static char* s_myfqdn(void)
{
	struct addrinfo hints, *info, *p;
	int rc;

	char nodename[1024];
	nodename[1023] = '\0';
	gethostname(nodename, 1023);

	memset(&hints, 0, sizeof hints);
	hints.ai_family   = AF_UNSPEC; /*either IPV4 or IPV6*/
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_CANONNAME;

	rc = getaddrinfo(nodename, NULL, &hints, &info);
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
	int rc;
	void *context  = zmq_ctx_new();
	void *listener = zmq_socket(context, ZMQ_ROUTER);
	zmq_bind(listener, "tcp://*:2324");
	cw_log_open("cogd", "stdout");
	cw_log(LOG_INFO, "cogd starting up");

	char *myfqdn = s_myfqdn();
	if (!myfqdn) exit(1);
	cw_log(LOG_INFO, "detected my FQDN as '%s'", myfqdn);

	int64_t next_run = cw_time_ms();
	while (!cw_sig_interrupt()) {
		zmq_pollitem_t socks[] = {
			{ listener, 0, ZMQ_POLLIN, 0 },
		};

		int time_left = (int)((next_run - cw_time_ms()));
		if (time_left < 0) time_left = 0;

		errno = 0;
		rc = zmq_poll(socks, 1, time_left);
		if (rc == -1)
			break;

		if (socks[0].revents & ZMQ_POLLIN) {
			cw_pdu_t *pdu, *reply;
			pdu = cw_pdu_recv(listener);
			assert(pdu);

			if (strcmp(pdu->type, "INFO") == 0) {
				char *code = cw_pdu_text(pdu, 1);
				cw_log(LOG_INFO, "inbound INFO request:\n%s", code);

				reply = cw_pdu_make(pdu->src, 2, "OK", "");
				cw_pdu_send(listener, reply);

			} else {
				cw_log(LOG_INFO, "unrecognized PDU: '%s'", pdu->type);
				reply = cw_pdu_make(pdu->src, 2, "ERROR", "Unrecognized PDU");
				cw_pdu_send(listener, reply);
			}
		}

		if (cw_time_ms() >= next_run) {
			cw_hash_t facts;
			memset(&facts, 0, sizeof(cw_hash_t));
			if (fact_gather("/lib/clockwork/gather.d/*", &facts) != 0) {
				cw_log(LOG_CRIT, "Unable to gather facts");
				goto maybe_next_time;
			}

			FILE *io = tmpfile();
			assert(io);
			rc = fact_write(io, &facts);
			assert(rc == 0);
			fprintf(io, "%c", '\0');
			size_t len = ftell(io);

			char *factstr = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(io), 0);
			if (!factstr) {
				cw_log(LOG_CRIT, "Failed to mmap fact data");
			}

			void *client = zmq_socket(context, ZMQ_DEALER);
			char *endpoint = strdup("tcp://localhost:2323");
			cw_log(LOG_INFO, "connecting to %s", endpoint);

			rc = zmq_connect(client, endpoint);
			assert(rc == 0);

			cw_pdu_t *pdu, *reply;
			pdu = cw_pdu_make(NULL, 3, "POLICY", myfqdn, factstr);
			rc = cw_pdu_send(client, pdu);

			munmap(factstr, len);
			fclose(io);

			/* fixme: guard against clockd going down! */
			reply = cw_pdu_recv(client);
			if (!reply) {
				cw_log(LOG_ERR, "failed: %s", zmq_strerror(errno));
			} else {
				char *code = cw_pdu_text(reply, 1);
				/*
				printf("Received PENDULUM code:\n"
				       "-----------------------\n%s\n\n",
				       code);
				*/

				pn_machine m;
				pn_init(&m);
				pendulum_funcs(&m);

				FILE *io = tmpfile();
				fprintf(io, "%s", code);
				fseek(io, 0, SEEK_SET);

				pn_parse(&m, io);
				pn_run(&m);
			}

			cw_zmq_shutdown(client, 0);
			cw_log(LOG_INFO, "closed connection");

	maybe_next_time:
			next_run = cw_time_ms() + INTERVAL;
		}
	}
	cw_log(LOG_INFO, "shutting down");

	cw_zmq_shutdown(listener, 500);
	zmq_ctx_destroy(context);
	return 0;
}
