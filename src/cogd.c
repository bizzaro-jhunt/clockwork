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
#include "cw.h"
#include <zmq.h>

#define INTERVAL 1000 * 5

int main(int argc, char **argv)
{
	int rc;
	void *context  = zmq_ctx_new();
	void *listener = zmq_socket(context, ZMQ_ROUTER);
	zmq_bind(listener, "tcp://*:2324");
	cw_log_open("cogd", "stdout");
	cw_log(LOG_INFO, "cogd starting up");

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

			if (strcmp(pdu->type, "POLICY") == 0) {
				char *name  = cw_pdu_text(pdu, 1);
				char *facts = cw_pdu_text(pdu, 2);
				cw_log(LOG_INFO, "inbound policy request for %s", name);

				reply = cw_pdu_make(pdu->src, 2, "OK", "NOOP\n");
				cw_pdu_send(listener, reply);

			} else {
				cw_log(LOG_INFO, "unrecognized PDU: '%s'", pdu->type);
				reply = cw_pdu_make(pdu->src, 2, "ERROR", "Unrecognized PDU");
				cw_pdu_send(listener, reply);
			}
		}

		if (cw_time_ms() >= next_run) {
			void *client = zmq_socket(context, ZMQ_DEALER);
			char *endpoint = strdup("tcp://localhost:2323");
			cw_log(LOG_INFO, "connecting to %s", endpoint);

			rc = zmq_connect(client, endpoint);
			assert(rc == 0);

			cw_pdu_t *pdu, *reply;
			pdu = cw_pdu_make(NULL, 3, "POLICY", "file2.test", "fact=value\n");
			rc = cw_pdu_send(client, pdu);

			/* fixme: guard against clockd going down! */
			reply = cw_pdu_recv(client);
			if (!reply) {
				cw_log(LOG_ERR, "failed: %s", zmq_strerror(errno));
			} else {
				char *code = cw_pdu_text(reply, 1);
				printf("Received PENDULUM code:\n"
				       "-----------------------\n%s\n\n",
				       code);
			}

			cw_zmq_shutdown(client, 0);
			cw_log(LOG_INFO, "closed connection");
			next_run = cw_time_ms() + INTERVAL;
		}
	}
	cw_log(LOG_INFO, "shutting down");

	cw_zmq_shutdown(listener, 500);
	zmq_ctx_destroy(context);
	return 0;
}
