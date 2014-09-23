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

#include <sodium.h>
#include "test.h"
#include "../src/clockwork.h"
#include "../src/mesh.h"

#include <unistd.h>
#include <pthread.h>

#define FQDN "test01.lab.example.com"
#define UUID1 "e6f0a5ea-cc63-4784-be50-0e37eb11c8b2"
#define UUID2 "7a2642e6-5722-4ccb-95e9-30a0e52cb1fb"
#define UUID3 "43033326-35f4-410c-a638-8f70047a7c16"

void* mesh_server_thread(void *data)
{
	mesh_server_run((mesh_server_t*)data);
	return NULL;
}

void* mesh_client_thread(void *ctx)
{
	mesh_client_t *client = mesh_client_new();

	cw_hash_t facts;
	memset(&facts, 0, sizeof(facts));

	cw_hash_set(&facts, "sys.fqdn", FQDN);
	cw_hash_set(&facts, "sys.os",   "linux");
	cw_hash_set(&facts, "sys.uuid", UUID1);

	int rc;
	rc = mesh_client_setopt(client, MESH_CLIENT_FACTS, &facts, sizeof(cw_hash_t*));
	assert(rc == 0);

	FILE *io = tmpfile();
	fprintf(io, "allow user1 'show *'\n");
	fprintf(io, "allow %%sys '*'\n");
	rewind(io);

	LIST(acl);
	rc = acl_readio(&acl, io);
	fclose(io);

	rc = mesh_client_setopt(client, MESH_CLIENT_ACL, &acl, sizeof(cw_list_t*));
	assert(rc == 0);

	int acl_default = ACL_DENY;
	rc = mesh_client_setopt(client, MESH_CLIENT_ACL_DEFAULT, &acl_default, sizeof(acl_default));
	assert(rc == 0);

	rc = mesh_client_setopt(client, MESH_CLIENT_FQDN, FQDN, strlen(FQDN));
	assert(rc == 0);

	void *broadcast = zmq_socket(ctx, ZMQ_SUB);
	rc = zmq_connect(broadcast, "inproc://mesh.broadcaster");
	assert(rc == 0);
	rc = zmq_setsockopt(broadcast, ZMQ_SUBSCRIBE, NULL, 0);
	assert(rc == 0);

	void *control = zmq_socket(ctx, ZMQ_DEALER);
	rc = zmq_connect(control, "inproc://mesh.control");
	assert(rc == 0);

	cw_pdu_t *pdu = cw_pdu_recv(broadcast);
	rc = mesh_client_handle(client, control, pdu);
	cw_pdu_destroy(pdu);

	cw_hash_done(&facts, 0);
	acl_t *a, *a_tmp;
	for_each_object_safe(a, a_tmp, &acl, l)
		acl_destroy(a);

	cw_zmq_shutdown(broadcast, 0);
	cw_zmq_shutdown(control,   0);
	mesh_client_destroy(client);

	return NULL;
}

TESTS {
	subtest { /* server reactor */
		char *s; int rc;

		void *ctx = zmq_ctx_new();
		mesh_server_t *server = mesh_server_new(ctx);

		uint64_t serial = 0x4242UL;
		ok(mesh_server_setopt(server, MESH_SERVER_SERIAL, &serial, sizeof(serial)) == 0,
			"Set a fixed serial number on the mesh_server");
		ok(mesh_server_setopt(server, MESH_SERVER_PAM_SERVICE, NULL, 0) == 0,
			"Disabled PAM authentication in the mesh_server");
		ok(mesh_server_setopt(server, MESH_SERVER_SAFE_WORD, "TEST_COMPLETE", 13) == 0,
			"Set a safe-word for early exit");

		ok(mesh_server_bind_control(server, "inproc://ctl.1") == 0,
			"Bound control port to inproc://ctl.1");
		ok(mesh_server_bind_broadcast(server, "inproc://pub.1") == 0,
			"Bound broadcast port to inproc://pub.1");

		pthread_t tid;
		rc = pthread_create(&tid, NULL, mesh_server_thread, server);
		is_int(rc, 0, "created meshd thread");

		void *sock = zmq_socket(server->zmq, ZMQ_DEALER);
		rc = zmq_connect(sock, "inproc://ctl.1");
		is_int(rc, 0, "connected to inproc://ctl.1");

		cw_pdu_t *pdu, *reply;
		pdu = cw_pdu_make(NULL, 6,
				"REQUEST", "anonymous", "", "nopassword",
				"show version", "" /* no filter */);
		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent REQUEST PDU to meshd thread");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = cw_pdu_text(reply, 0), "SUBMITTED", "REQUEST -> SUBMITTED"); free(s);
		is_string(s = cw_pdu_text(reply, 1), "4242", "Serial returned to client"); free(s);
		is_null(cw_pdu_text(reply, 2), "There is no 3rd frame");
		cw_pdu_destroy(reply);

		/* simulate back-channel responses */
		pdu = cw_pdu_make(NULL, 6,
				"RESULT",
				"4242",        /* request serial */
				"host1.fq.dn", /* node FQDN */
				UUID1,
				"0",           /* status code */
				"1.2.3\n");    /* output */
		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent RESULT PDU to meshd thread");
		cw_pdu_destroy(pdu);

		/* check our data */
		pdu = cw_pdu_make(NULL, 2, "CHECK", "4242");
		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent a CHECK PDU");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = cw_pdu_text(reply, 0), "RESULT",      "first reply is a RESULT");    free(s);
		is_string(s = cw_pdu_text(reply, 1), "host1.fq.dn", "first reply is from host1");  free(s);
		is_string(s = cw_pdu_text(reply, 2), UUID1,         "host1 UUID in frame 2");      free(s);
		is_string(s = cw_pdu_text(reply, 3), "0",           "first reply was OK (rc 0)");  free(s);
		is_string(s = cw_pdu_text(reply, 4), "1.2.3\n",     "first reply output proxied"); free(s);
		is_null(cw_pdu_text(reply, 5), "there is no 5th frame in a RESULT PDU");
		cw_pdu_destroy(reply);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = cw_pdu_text(reply, 0), "DONE", "end of transmission"); free(s);
		is_null(cw_pdu_text(reply, 1), "there is no 2nd frame in a DONE PDU");
		cw_pdu_destroy(reply);

		/* send more data from backend nodes */

		pdu = cw_pdu_make(NULL, 6,
				"RESULT",
				"4242",
				"host2.fq.dn",
				UUID2,
				"0",
				"1.2.5\n");

		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent another RESULT PDU from host2.fq.dn");
		cw_pdu_destroy(pdu);

		pdu = cw_pdu_make(NULL, 4, "OPTOUT", "4242", "host3.fq.dn", UUID3);
		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent an OPTOUT PDU from host3.fq.dn");
		cw_pdu_destroy(pdu);

		/* check our data */
		pdu = cw_pdu_make(NULL, 2, "CHECK", "4242");
		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent a CHECK PDU");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = cw_pdu_text(reply, 0), "RESULT",      "first reply is a RESULT");    free(s);
		is_string(s = cw_pdu_text(reply, 1), "host2.fq.dn", "first reply is from host2");  free(s);
		is_string(s = cw_pdu_text(reply, 2), UUID2,         "host2 UUID supplied");  free(s);
		is_string(s = cw_pdu_text(reply, 3), "0",           "first reply was OK (rc 0)");  free(s);
		is_string(s = cw_pdu_text(reply, 4), "1.2.5\n",     "first reply output proxied"); free(s);
		is_null(cw_pdu_text(reply, 5), "there is no sixth frame in a RESULT PDU");
		cw_pdu_destroy(reply);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = cw_pdu_text(reply, 0), "OPTOUT",      "second reply is a OPTOUT");   free(s);
		is_string(s = cw_pdu_text(reply, 1), "host3.fq.dn", "second reply is from host3"); free(s);
		is_string(s = cw_pdu_text(reply, 2), UUID3,         "host3 UUID"); free(s);
		is_null(cw_pdu_text(reply, 3), "there is no fourth frame in an OPTOUT PDU");
		cw_pdu_destroy(reply);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = cw_pdu_text(reply, 0), "DONE", "end of transmission"); free(s);
		is_null(cw_pdu_text(reply, 1), "there is no 2nd frame in a DONE PDU");
		cw_pdu_destroy(reply);

		/* send the safe word to terminate the meshd thread */
		pdu = cw_pdu_make(NULL, 1, "TEST_COMPLETE");
		cw_pdu_send(sock, pdu);
		cw_pdu_destroy(pdu);

		void *_;
		pthread_join(tid, &_);
		cw_zmq_shutdown(sock, 0);
		mesh_server_destroy(server);
		zmq_ctx_destroy(ctx);
	}

	subtest {
		int rc;
		char *s;
		void *ctx = zmq_ctx_new();

		void *broadcast = zmq_socket(ctx, ZMQ_PUB);
		rc = zmq_bind(broadcast, "inproc://mesh.broadcaster");
		is_int(rc, 0, "bound inproc://mesh.broadcaster");

		void *control = zmq_socket(ctx, ZMQ_ROUTER);
		rc = zmq_bind(control, "inproc://mesh.control");
		is_int(rc, 0, "bound inproc://mesh.control");

		pthread_t tid;
		rc = pthread_create(&tid, NULL, mesh_client_thread, ctx);
		is_int(rc, 0, "created client thread");
		cw_sleep_ms(600);

		/* send the command */
		cw_pdu_t *pdu = cw_pdu_make(NULL, 6,
				"COMMAND",
				"4242",                 /* serial */
				"user1:group2:group3",  /* credentials */
				"show version",         /* command string */
				";; pn\nSHOW version",  /* pendulum code */
				"");                    /* no filters */
		rc = cw_pdu_send(broadcast, pdu);
		is_int(rc, 0, "sent COMMAND PDU to subscribers");
		cw_pdu_destroy(pdu);

		cw_pdu_t *reply = cw_pdu_recv(control);
		isnt_null(reply, "Got a reply from the mesh client thread");
		is_string(reply->type, "RESULT", "reply was a RESULT");
		is_string(s = cw_pdu_text(reply, 1), "4242", "serial echoed back in frame 1");   free(s);
		is_string(s = cw_pdu_text(reply, 2), FQDN,   "mesh node FQDN in frame 2");       free(s);
		is_string(s = cw_pdu_text(reply, 3), UUID1,  "mesh node UUID in frame 3");       free(s);
		is_string(s = cw_pdu_text(reply, 4), "0",    "status code is in frame 4");       free(s);
		is_string(s = cw_pdu_text(reply, 5), PACKAGE_VERSION "\n", "output in frame 5"); free(s);
		is_null(cw_pdu_text(reply, 6), "there is no sixth frame");
		cw_pdu_destroy(reply);

		void *_;
		pthread_join(tid, &_);

		cw_zmq_shutdown(broadcast, 500);
		cw_zmq_shutdown(control,   500);
		zmq_ctx_destroy(ctx);
	}

	subtest { /* key-based authentication */
		char *trustdb_path = strdup(TEST_TMP "/data/mesh/trustdb");
		char *authkey_path = strdup(TEST_TMP "/data/mesh/key.trusted");

		char *s; int rc;

		void *ctx = zmq_ctx_new();
		mesh_server_t *server = mesh_server_new(ctx);

		uint64_t serial = 0x4242UL;
		ok(mesh_server_setopt(server, MESH_SERVER_SERIAL, &serial, sizeof(serial)) == 0,
			"Set a fixed serial number on the mesh_server");
		ok(mesh_server_setopt(server, MESH_SERVER_PAM_SERVICE, NULL, 0) == 0,
			"Disabled PAM authentication in the mesh_server");
		ok(mesh_server_setopt(server, MESH_SERVER_TRUSTDB, trustdb_path, strlen(trustdb_path)) == 0,
			"Set trust database path for key-based auth");
		ok(mesh_server_setopt(server, MESH_SERVER_SAFE_WORD, "TEST_COMPLETE", 13) == 0,
			"Set a safe-word for early exit");

		ok(mesh_server_bind_control(server, "inproc://ctl.1") == 0,
			"Bound control port to inproc://ctl.1");
		ok(mesh_server_bind_broadcast(server, "inproc://pub.1") == 0,
			"Bound broadcast port to inproc://pub.1");

		pthread_t tid;
		rc = pthread_create(&tid, NULL, mesh_server_thread, server);
		is_int(rc, 0, "created meshd thread");

		void *sock = zmq_socket(server->zmq, ZMQ_DEALER);
		rc = zmq_connect(sock, "inproc://ctl.1");
		is_int(rc, 0, "connected to inproc://ctl.1");

		cw_cert_t *authkey = cw_cert_read(authkey_path);
		isnt_null(authkey, "Read authkey from test data");

		unsigned char unsealed[256 - 64];
		randombytes(unsealed, 256 - 64);
		uint8_t *sealed;
		unsigned long long slen;
		slen = cw_cert_seal(authkey, unsealed, 256 - 64, &sealed);
		is_int(slen, 256, "sealed message is 256 bytes long");

		cw_pdu_t *pdu, *reply;
		pdu = cw_pdu_make(NULL, 2, "REQUEST", "jhacker");
		char *pubkey = cw_cert_public_s(authkey);
		cw_pdu_extend(pdu, cw_frame_new(pubkey));
		cw_pdu_extend(pdu, cw_frame_newbuf((const char*)sealed, 256));
		free(pubkey);
		free(sealed);
		cw_pdu_extend(pdu, cw_frame_new("show version"));
		cw_pdu_extend(pdu, cw_frame_new("")); /* no filter */

		cw_cert_destroy(authkey);

		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent REQUEST PDU to meshd thread");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = cw_pdu_text(reply, 0), "SUBMITTED", "REQUEST -> SUBMITTED"); free(s);
		is_string(s = cw_pdu_text(reply, 1), "4242", "Serial returned to client"); free(s);
		is_null(cw_pdu_text(reply, 2), "There is no 3rd frame");
		cw_pdu_destroy(reply);

		/* send the safe word to terminate the meshd thread */
		pdu = cw_pdu_make(NULL, 1, "TEST_COMPLETE");
		cw_pdu_send(sock, pdu);
		cw_pdu_destroy(pdu);

		void *_;
		pthread_join(tid, &_);
		cw_zmq_shutdown(sock, 0);
		mesh_server_destroy(server);
		zmq_ctx_destroy(ctx);
		free(authkey_path);
		free(trustdb_path);
	}

	subtest { /* key-based authentication (username mismatch fail) */
		char *trustdb_path = strdup(TEST_TMP "/data/mesh/trustdb");
		char *authkey_path = strdup(TEST_TMP "/data/mesh/key.trusted");

		char *s; int rc;

		void *ctx = zmq_ctx_new();
		mesh_server_t *server = mesh_server_new(ctx);

		ok(mesh_server_setopt(server, MESH_SERVER_TRUSTDB, trustdb_path, strlen(trustdb_path)) == 0,
			"Set trust database path for key-based auth");
		ok(mesh_server_setopt(server, MESH_SERVER_SAFE_WORD, "TEST_COMPLETE", 13) == 0,
			"Set a safe-word for early exit");

		ok(mesh_server_bind_control(server, "inproc://ctl.1") == 0,
			"Bound control port to inproc://ctl.1");
		ok(mesh_server_bind_broadcast(server, "inproc://pub.1") == 0,
			"Bound broadcast port to inproc://pub.1");

		pthread_t tid;
		rc = pthread_create(&tid, NULL, mesh_server_thread, server);
		is_int(rc, 0, "created meshd thread");

		void *sock = zmq_socket(server->zmq, ZMQ_DEALER);
		rc = zmq_connect(sock, "inproc://ctl.1");
		is_int(rc, 0, "connected to inproc://ctl.1");

		cw_cert_t *authkey = cw_cert_read(authkey_path);
		isnt_null(authkey, "Read authkey from test data");

		unsigned char unsealed[256 - 64];
		randombytes(unsealed, 256 - 64);
		uint8_t *sealed;
		unsigned long long slen;
		slen = cw_cert_seal(authkey, unsealed, 256 - 64, &sealed);
		is_int(slen, 256, "sealed message is 256 bytes long");

		cw_pdu_t *pdu, *reply;
		pdu = cw_pdu_make(NULL, 2, "REQUEST", "WrongUser");
		char *pubkey = cw_cert_public_s(authkey);
		cw_pdu_extend(pdu, cw_frame_new(pubkey));
		cw_pdu_extend(pdu, cw_frame_newbuf((const char*)sealed, 256));
		free(pubkey);
		free(sealed);
		cw_pdu_extend(pdu, cw_frame_new("show version"));
		cw_pdu_extend(pdu, cw_frame_new("")); /* no filter */

		cw_cert_destroy(authkey);

		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent REQUEST PDU to meshd thread");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = cw_pdu_text(reply, 0), "ERROR", "REQUEST -> ERROR"); free(s);
		is_string(s = cw_pdu_text(reply, 1), "Authentication failed (pubkey)",
				"Authentication failed (username mismatch)"); free(s);
		is_null(cw_pdu_text(reply, 2), "There is no 3rd frame");
		cw_pdu_destroy(reply);

		/* send the safe word to terminate the meshd thread */
		pdu = cw_pdu_make(NULL, 1, "TEST_COMPLETE");
		cw_pdu_send(sock, pdu);
		cw_pdu_destroy(pdu);

		void *_;
		pthread_join(tid, &_);
		cw_zmq_shutdown(sock, 0);
		mesh_server_destroy(server);
		zmq_ctx_destroy(ctx);

		free(authkey_path);
		free(trustdb_path);
	}

	subtest { /* key-based authentication (untrusted key) */
		char *trustdb_path = strdup(TEST_TMP "/data/mesh/trustdb");
		char *authkey_path = strdup(TEST_TMP "/data/mesh/key.untrusted");

		char *s; int rc;

		void *ctx = zmq_ctx_new();
		mesh_server_t *server = mesh_server_new(ctx);

		ok(mesh_server_setopt(server, MESH_SERVER_TRUSTDB, trustdb_path, strlen(trustdb_path)) == 0,
			"Set trust database path for key-based auth");
		ok(mesh_server_setopt(server, MESH_SERVER_SAFE_WORD, "TEST_COMPLETE", 13) == 0,
			"Set a safe-word for early exit");

		ok(mesh_server_bind_control(server, "inproc://ctl.1") == 0,
			"Bound control port to inproc://ctl.1");
		ok(mesh_server_bind_broadcast(server, "inproc://pub.1") == 0,
			"Bound broadcast port to inproc://pub.1");

		pthread_t tid;
		rc = pthread_create(&tid, NULL, mesh_server_thread, server);
		is_int(rc, 0, "created meshd thread");

		void *sock = zmq_socket(server->zmq, ZMQ_DEALER);
		rc = zmq_connect(sock, "inproc://ctl.1");
		is_int(rc, 0, "connected to inproc://ctl.1");

		cw_cert_t *authkey = cw_cert_read(authkey_path);
		isnt_null(authkey, "Read authkey from test data");

		unsigned char unsealed[256 - 64];
		randombytes(unsealed, 256 - 64);
		uint8_t *sealed;
		unsigned long long slen;
		slen = cw_cert_seal(authkey, unsealed, 256 - 64, &sealed);
		is_int(slen, 256, "sealed message is 256 bytes long");

		cw_pdu_t *pdu, *reply;
		pdu = cw_pdu_make(NULL, 2, "REQUEST", "rogue");
		char *pubkey = cw_cert_public_s(authkey);
		cw_pdu_extend(pdu, cw_frame_new(pubkey));
		cw_pdu_extend(pdu, cw_frame_newbuf((const char*)sealed, 256));
		free(pubkey);
		free(sealed);
		cw_pdu_extend(pdu, cw_frame_new("show version"));
		cw_pdu_extend(pdu, cw_frame_new("")); /* no filter */

		cw_cert_destroy(authkey);

		rc = cw_pdu_send(sock, pdu);
		is_int(rc, 0, "sent REQUEST PDU to meshd thread");
		cw_pdu_destroy(pdu);

		reply = cw_pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = cw_pdu_text(reply, 0), "ERROR", "REQUEST -> ERROR"); free(s);
		is_string(s = cw_pdu_text(reply, 1), "Authentication failed (pubkey)",
				"Authentication failed (untrusted key)"); free(s);
		is_null(cw_pdu_text(reply, 2), "There is no 3rd frame");
		cw_pdu_destroy(reply);

		/* send the safe word to terminate the meshd thread */
		pdu = cw_pdu_make(NULL, 1, "TEST_COMPLETE");
		cw_pdu_send(sock, pdu);
		cw_pdu_destroy(pdu);

		void *_;
		pthread_join(tid, &_);
		cw_zmq_shutdown(sock, 0);
		mesh_server_destroy(server);
		zmq_ctx_destroy(ctx);

		free(authkey_path);
		free(trustdb_path);
	}

	done_testing();
}
