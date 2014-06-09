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

#include "test.h"
#include "../src/cw.h"

TESTS {
	subtest {
		cw_pdu_t *pdu;
		pdu = cw_pdu_make(NULL, 2, "PING", "... payload ...");
		isnt_null(pdu, "cw_pdu_make generated a PDU");

		char *s;
		is_string((s = cw_pdu_text(pdu, 0)), "PING", "first frame of pdu 1");
		free(s);
		is_string((s = cw_pdu_text(pdu, 0)), "PING", "first frame of pdu 1 (post caller-free())");
		free(s);

		is_string((s = cw_pdu_text(pdu, 1)), "... payload ...", "second frame of pdu 1");
		free(s);

		is_null(cw_pdu_text(pdu, 2), "pdu 1 has no 3rd frame");

		cw_pdu_destroy(pdu);
	}

	subtest {
		void *ctx = zmq_ctx_new();
		isnt_null(ctx, "Generated a new 0MQ context");

		void *client = zmq_socket(ctx, ZMQ_REQ);
		void *server = zmq_socket(ctx, ZMQ_REP);

		int rc;
		rc = zmq_bind(server, "inproc://testing");
		is_int(rc, 0, "bound server to inproc://testing");
		assert(rc == 0);

		rc = zmq_connect(client, "inproc://testing");
		is_int(rc, 0, "bound client to inproc://testing");
		assert(rc == 0);

		/* NB: we do very bad things here since (re: distributed
			   message-passing system architecture), since we are
			   just using 0MQ as a substrate to test cw_pdu_*.
		
			   In other words, don't do this in real code. */
		cw_pdu_t *pdu, *reply;
		char *s;

		diag("client sending PING");
		pdu = cw_pdu_make(NULL, 1, "PING");
		rc = cw_pdu_send(client, pdu);
		cw_pdu_destroy(pdu);

		diag("server receiving PING");
		pdu = cw_pdu_recv(server);
		is_string((s = cw_pdu_text(pdu, 0)), "PING", "client -> server: PING");
		free(s);

		diag("server sending PONG");
		reply = cw_pdu_make(pdu->src, 1, "PONG");
		rc = cw_pdu_send(server, reply);
		cw_pdu_destroy(pdu);
		cw_pdu_destroy(reply);

		diag("client receiving PONG");
		pdu = cw_pdu_recv(client);
		is_string((s = cw_pdu_text(pdu, 0)), "PONG", "server -> client: PONG");
		free(s);
		cw_pdu_destroy(pdu);

		/* fin */
		cw_zmq_shutdown(client, 0);
		cw_zmq_shutdown(server, 0);
		zmq_ctx_destroy(ctx);
	}

	done_testing();
}
