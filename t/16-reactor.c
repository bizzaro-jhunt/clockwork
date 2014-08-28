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
#include <pthread.h>

int reactor_echo(void *sock, cw_pdu_t *pdu, void *data)
{
	cw_pdu_t *reply;
	int rc = CW_REACTOR_CONTINUE;

	if (strcmp(pdu->type, "ECHO") == 0) {
		char *payload = cw_pdu_text(pdu, 1);
		reply = cw_pdu_make(pdu->src, 2, "ECHO", payload);
		free(payload);

	} else if (strcmp(pdu->type, "EXIT") == 0) {
		reply = cw_pdu_make(pdu->src, 1, "BYE");
		rc = 0;

	} else {
		reply = cw_pdu_make(pdu->src, 2, "ERROR", "bad PDU type");
	}

	cw_pdu_send(sock, reply);
	cw_pdu_destroy(reply);
	return rc;
}

int reactor_reverse(void *sock, cw_pdu_t *pdu, void *data)
{
	cw_pdu_t *reply;
	int rc = CW_REACTOR_CONTINUE;

	if (strcmp(pdu->type, "EXIT") == 0) {
		reply = cw_pdu_make(pdu->src, 1, "BYE");
		rc = 0;

	} else {
		char *reversed = cw_pdu_text(pdu, 0);
		if (*reversed != '\0') {
			int i, len = strlen(reversed);
			for (i = 0; i < len / 2; i++) {
				char t = reversed[i];
				reversed[i] = reversed[len - i - 1];
				reversed[len - i - 1] = t;
			}
		}
		reply = cw_pdu_make(pdu->src, 1, reversed);
		free(reversed);
	}

	cw_pdu_send(sock, reply);
	cw_pdu_destroy(reply);
	return rc;
}

void* server1_thread(void *Z)
{
	int rc;
	void *sock = zmq_socket(Z, ZMQ_ROUTER);
	rc = zmq_bind(sock, "inproc://reactor.16.server1");
	if (rc != 0) return cw_string("failed to bind: %s", zmq_strerror(errno));

	cw_reactor_t *r = cw_reactor_new();
	if (!r) return strdup("failed to create a reactor");

	rc = cw_reactor_add(r, sock, reactor_echo, NULL);
	if (rc != 0) return cw_string("failed to add handler to reactor (rc=%i)", rc);

	rc = cw_reactor_loop(r);
	if (rc != 0) return cw_string("cw_reactor_loop returned non-zero (rc=%i)", rc);

	cw_reactor_destroy(r);
	cw_zmq_shutdown(sock, 500);
	return strdup("OK");
}

void *server2_thread(void *Z)
{
	int rc;
	void *echoer = zmq_socket(Z, ZMQ_ROUTER);
	rc = zmq_bind(echoer, "inproc://reactor.16.server2.echo");
	if (rc != 0) return cw_string("failed to bind echoer: %s", zmq_strerror(errno));

	void *reverser = zmq_socket(Z, ZMQ_ROUTER);
	rc = zmq_bind(reverser, "inproc://reactor.16.server2.reverse");
	if (rc != 0) return cw_string("failed to bind reverser: %s", zmq_strerror(errno));

	cw_reactor_t *r = cw_reactor_new();
	if (!r) return strdup("failed to create a reactor");

	rc = cw_reactor_add(r, echoer, reactor_echo, NULL);
	if (rc != 0) return cw_string("failed to add echo handler (rc=%i)", rc);
	rc = cw_reactor_add(r, reverser, reactor_reverse, NULL);
	if (rc != 0) return cw_string("failed to add reverse handler (rc=%i)", rc);

	rc = cw_reactor_loop(r);
	if (rc != 0) return cw_string("cw_reactor_loop returned non-zero (rc=%i)", rc);

	cw_reactor_destroy(r);
	cw_zmq_shutdown(echoer,   500);
	cw_zmq_shutdown(reverser, 500);
	return strdup("OK");
}

TESTS {
	void *Z = zmq_ctx_new();
	char *s;

	subtest { /* whitebox testing */
		cw_reactor_t *r = cw_reactor_new();
		isnt_null(r, "cw_reactor_new() gave us a new reactor handle");
		cw_reactor_destroy(r);
	}

	subtest { /* full-stack client */
		pthread_t tid;
		int rc = pthread_create(&tid, NULL, server1_thread, Z);
		is_int(rc, 0, "created server1 thread");
		cw_sleep_ms(250);

		void *sock = zmq_socket(Z, ZMQ_DEALER);
		rc = zmq_connect(sock, "inproc://reactor.16.server1");
		is_int(rc, 0, "connected to inproc:// endpoint");

		cw_pdu_t *pdu, *reply;
		pdu = cw_pdu_make(NULL, 2, "ECHO", "Are you there, Reactor? It's me, Tester");
		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent ECHO PDU to server1_thread");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from server1_thread");
		is_string(s = cw_pdu_text(reply, 0), "ECHO", "ECHO yields ECHO"); free(s);
		is_string(s = cw_pdu_text(reply, 1), "Are you there, Reactor? It's me, Tester",
			"server1_thread echoed back our PDU"); free(s);
		cw_pdu_destroy(reply);

		pdu = cw_pdu_make(NULL, 1, "EXIT");
		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent EXIT PDU to server1_thread");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from server1_thread");
		is_string(s = cw_pdu_text(reply, 0), "BYE", "EXIT yields BYE"); free(s);
		cw_pdu_destroy(reply);

		char *result = NULL;
		pthread_join(tid, (void**)(&result));
		is_string(result, "OK", "server1_thread returned OK");
		free(result);

		cw_zmq_shutdown(sock, 0);
	}

	subtest { /* multiple listening sockets */
		pthread_t tid;
		int rc = pthread_create(&tid, NULL, server2_thread, Z);
		is_int(rc, 0, "created server2 thread");
		cw_sleep_ms(250);

		void *echo = zmq_socket(Z, ZMQ_DEALER);
		rc = zmq_connect(echo, "inproc://reactor.16.server2.echo");
		is_int(rc, 0, "connected to inproc://echo endpoint");

		void *reverse = zmq_socket(Z, ZMQ_DEALER);
		rc = zmq_connect(reverse, "inproc://reactor.16.server2.reverse");
		is_int(rc, 0, "connected to inproc://reverse endpoint");

		cw_pdu_t *pdu, *reply;
		pdu = cw_pdu_make(NULL, 2, "ECHO", "XYZZY");
		rc = cw_pdu_send(echo, pdu);
		is_int(rc, 0, "sent ECHO PDU to server2_thread:echo");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(echo);
		isnt_null(reply, "Got a reply from server2_thread");
		is_string(s = cw_pdu_text(reply, 0), "ECHO", "ECHO yields ECHO"); free(s);
		is_string(s = cw_pdu_text(reply, 1), "XYZZY", "server2_thread echoed back our PDU"); free(s);
		cw_pdu_destroy(reply);

		pdu = cw_pdu_make(NULL, 1, "Rewind and let me reverse it");
		rc = cw_pdu_send(reverse, pdu);
		is_int(rc, 0, "sent a PDU to the server2_thread:reverse");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(reverse);
		isnt_null(reply, "Got a reply from server2_thread");
		is_string(s = cw_pdu_text(reply, 0), "ti esrever em tel dna dniweR",
				"Reverser socket got our PDU and did the needful"); free(s);
		cw_pdu_destroy(reply);

		pdu = cw_pdu_make(NULL, 1, "EXIT");
		rc = cw_pdu_send(echo, pdu);
		is_int(rc, 0, "sent EXIT PDU to server2_thread");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(echo);
		isnt_null(reply, "Got a reply from server2_thread");
		is_string(s = cw_pdu_text(reply, 0), "BYE", "EXIT yields BYE"); free(s);
		cw_pdu_destroy(reply);

		char *result = NULL;
		pthread_join(tid, (void**)(&result));
		is_string(result, "OK", "server2_thread returned OK");
		free(result);

		cw_zmq_shutdown(echo,    0);
		cw_zmq_shutdown(reverse, 0);
	}

	zmq_ctx_destroy(Z);
	done_testing();
}
