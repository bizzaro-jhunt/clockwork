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

	hash_t facts;
	memset(&facts, 0, sizeof(facts));

	hash_set(&facts, "sys.fqdn", FQDN);
	hash_set(&facts, "sys.os",   "linux");
	hash_set(&facts, "sys.uuid", UUID1);

	int rc;
	rc = mesh_client_setopt(client, MESH_CLIENT_FACTS, &facts, sizeof(hash_t*));
	assert(rc == 0);

	FILE *io = tmpfile();
	fprintf(io, "allow user1 'show *'\n");
	fprintf(io, "allow %%sys '*'\n");
	rewind(io);

	LIST(acl);
	rc = acl_readio(&acl, io);
	fclose(io);

	rc = mesh_client_setopt(client, MESH_CLIENT_ACL, &acl, sizeof(list_t*));
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

	pdu_t *pdu = pdu_recv(broadcast);
	rc = mesh_client_handle(client, control, pdu);
	pdu_free(pdu);

	hash_done(&facts, 0);
	acl_t *a, *a_tmp;
	for_each_object_safe(a, a_tmp, &acl, l)
		acl_destroy(a);

	vzmq_shutdown(broadcast, 0);
	vzmq_shutdown(control,   0);
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

		pdu_t *pdu, *reply;
		pdu = pdu_make("REQUEST", 5,
				"anonymous", "", "nopassword",
				"show version", "" /* no filter */);
		rc = pdu_send_and_free(pdu, sock);
		is_int(rc, 0, "sent REQUEST PDU to meshd thread");

		reply = pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = pdu_string(reply, 0), "SUBMITTED", "REQUEST -> SUBMITTED"); free(s);
		is_string(s = pdu_string(reply, 1), "4242", "Serial returned to client"); free(s);
		is_null(pdu_string(reply, 2), "There is no 3rd frame");
		pdu_free(reply);

		/* simulate back-channel responses */
		pdu = pdu_make("RESULT", 5,
				"4242",        /* request serial */
				"host1.fq.dn", /* node FQDN */
				UUID1,
				"0",           /* status code */
				"1.2.3\n");    /* output */
		rc = pdu_send_and_free(pdu, sock);
		is_int(rc, 0, "sent RESULT PDU to meshd thread");

		/* check our data */
		pdu = pdu_make("CHECK", 1, "4242");
		rc = pdu_send_and_free(pdu, sock);
		is_int(rc, 0, "sent a CHECK PDU");

		reply = pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = pdu_string(reply, 0), "RESULT",      "first reply is a RESULT");    free(s);
		is_string(s = pdu_string(reply, 1), "host1.fq.dn", "first reply is from host1");  free(s);
		is_string(s = pdu_string(reply, 2), UUID1,         "host1 UUID in frame 2");      free(s);
		is_string(s = pdu_string(reply, 3), "0",           "first reply was OK (rc 0)");  free(s);
		is_string(s = pdu_string(reply, 4), "1.2.3\n",     "first reply output proxied"); free(s);
		is_null(pdu_string(reply, 5), "there is no 5th frame in a RESULT PDU");
		pdu_free(reply);

		reply = pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = pdu_string(reply, 0), "DONE", "end of transmission"); free(s);
		is_null(pdu_string(reply, 1), "there is no 2nd frame in a DONE PDU");
		pdu_free(reply);

		/* send more data from backend nodes */

		pdu = pdu_make("RESULT", 5,
				"4242",
				"host2.fq.dn",
				UUID2,
				"0",
				"1.2.5\n");

		rc = pdu_send_and_free(pdu, sock);
		is_int(rc, 0, "sent another RESULT PDU from host2.fq.dn");

		pdu = pdu_make("OPTOUT", 3, "4242", "host3.fq.dn", UUID3);
		rc = pdu_send_and_free(pdu, sock);
		is_int(rc, 0, "sent an OPTOUT PDU from host3.fq.dn");

		/* check our data */
		pdu = pdu_make("CHECK", 1, "4242");
		rc = pdu_send_and_free(pdu, sock);
		is_int(rc, 0, "sent a CHECK PDU");

		reply = pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = pdu_string(reply, 0), "RESULT",      "first reply is a RESULT");    free(s);
		is_string(s = pdu_string(reply, 1), "host2.fq.dn", "first reply is from host2");  free(s);
		is_string(s = pdu_string(reply, 2), UUID2,         "host2 UUID supplied");        free(s);
		is_string(s = pdu_string(reply, 3), "0",           "first reply was OK (rc 0)");  free(s);
		is_string(s = pdu_string(reply, 4), "1.2.5\n",     "first reply output proxied"); free(s);
		is_null(pdu_string(reply, 5), "there is no sixth frame in a RESULT PDU");
		pdu_free(reply);

		reply = pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = pdu_string(reply, 0), "OPTOUT",      "second reply is a OPTOUT");   free(s);
		is_string(s = pdu_string(reply, 1), "host3.fq.dn", "second reply is from host3"); free(s);
		is_string(s = pdu_string(reply, 2), UUID3,         "host3 UUID"); free(s);
		is_null(pdu_string(reply, 3), "there is no fourth frame in an OPTOUT PDU");
		pdu_free(reply);

		reply = pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = pdu_string(reply, 0), "DONE", "end of transmission"); free(s);
		is_null(pdu_string(reply, 1), "there is no 2nd frame in a DONE PDU");
		pdu_free(reply);

		/* send the safe word to terminate the meshd thread */
		pdu = pdu_make("TEST_COMPLETE", 0);
		pdu_send_and_free(pdu, sock);

		void *_;
		pthread_join(tid, &_);
		vzmq_shutdown(sock, 0);
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
		sleep_ms(600);

		/* send the command */
		pdu_t *pdu = pdu_make("COMMAND", 5,
				"4242",                 /* serial */
				"user1:group2:group3",  /* credentials */
				"show version",         /* command string */
				";; pn\nSHOW version",  /* pendulum code */
				"");                    /* no filters */
		rc = pdu_send_and_free(pdu, broadcast);
		is_int(rc, 0, "sent COMMAND PDU to subscribers");

		pdu_t *reply = pdu_recv(control);
		isnt_null(reply, "Got a reply from the mesh client thread");
		is_string(pdu_type(reply), "RESULT", "reply was a RESULT");
		is_string(s = pdu_string(reply, 1), "4242", "serial echoed back in frame 1");   free(s);
		is_string(s = pdu_string(reply, 2), FQDN,   "mesh node FQDN in frame 2");       free(s);
		is_string(s = pdu_string(reply, 3), UUID1,  "mesh node UUID in frame 3");       free(s);
		is_string(s = pdu_string(reply, 4), "0",    "status code is in frame 4");       free(s);
		is_string(s = pdu_string(reply, 5), PACKAGE_VERSION "\n", "output in frame 5"); free(s);
		is_null(pdu_string(reply, 6), "there is no sixth frame");
		pdu_free(reply);

		void *_;
		pthread_join(tid, &_);

		vzmq_shutdown(broadcast, 500);
		vzmq_shutdown(control,   500);
		zmq_ctx_destroy(ctx);
	}

	subtest { /* key-based authentication */
		mkdir("t/tmp", 0777);

		char *trustdb_path = strdup("t/tmp/trustdb");
		char *authkey_path = strdup("t/tmp/key.trusted");

		put_file(trustdb_path, 0644,
		         "cf1fc3e3117ec452e216e8559ac5076f1586cf4e81b674e4428e5c3ced409169 jhacker\n");
		put_file(authkey_path, 0644,
		         "%signing v1\n"
		         "id  jhacker\n"
		         "pub cf1fc3e3117ec452e216e8559ac5076f1586cf4e81b674e4428e5c3ced409169\n"
		         "sec dddb0ae13f19fcb697a5f69334fa326de5af8e87a72bfc5a19793137a3aa5526cf1fc3e3117ec452e216e8559ac5076f1586cf4e81b674e4428e5c3ced409169\n");


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

		cert_t *authkey = cert_read(authkey_path);
		isnt_null(authkey, "Read authkey from test data");
		if (!authkey) BAIL_OUT("failed to read authkey");

		unsigned char unsealed[256 - 64];
		randombytes(unsealed, 256 - 64);
		uint8_t *sealed;
		unsigned long long slen;
		slen = cert_seal(authkey, unsealed, 256 - 64, &sealed);
		is_int(slen, 256, "sealed message is 256 bytes long");

		pdu_t *pdu, *reply;
		pdu = pdu_make("REQUEST", 1, "jhacker");
		char *pubkey = cert_public_s(authkey);
		pdu_extendf(pdu, "%s", pubkey); free(pubkey);
		pdu_extend( pdu, sealed, 256);  free(sealed);
		pdu_extendf(pdu, "show version");
		pdu_extendf(pdu, ""); /* no filter */

		cert_free(authkey);

		rc = pdu_send_and_free(pdu, sock);
		is_int(rc, 0, "sent REQUEST PDU to meshd thread");

		reply = pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = pdu_string(reply, 0), "SUBMITTED", "REQUEST -> SUBMITTED"); free(s);
		is_string(s = pdu_string(reply, 1), "4242", "Serial returned to client"); free(s);
		is_null(pdu_string(reply, 2), "There is no 3rd frame");
		pdu_free(reply);

		/* send the safe word to terminate the meshd thread */
		pdu = pdu_make("TEST_COMPLETE", 0);
		pdu_send_and_free(pdu, sock);

		void *_;
		pthread_join(tid, &_);
		vzmq_shutdown(sock, 0);
		mesh_server_destroy(server);
		zmq_ctx_destroy(ctx);
		free(authkey_path);
		free(trustdb_path);
	}

	subtest { /* key-based authentication (username mismatch fail) */
		mkdir("t/tmp", 0777);

		char *trustdb_path = strdup("t/tmp/trustdb");
		char *authkey_path = strdup("t/tmp/key.trusted");

		put_file(trustdb_path, 0644,
		         "cf1fc3e3117ec452e216e8559ac5076f1586cf4e81b674e4428e5c3ced409169 jhacker\n");
		put_file(authkey_path, 0644,
		         "%signing v1\n"
		         "id  jhacker\n"
		         "pub cf1fc3e3117ec452e216e8559ac5076f1586cf4e81b674e4428e5c3ced409169\n"
		         "sec dddb0ae13f19fcb697a5f69334fa326de5af8e87a72bfc5a19793137a3aa5526cf1fc3e3117ec452e216e8559ac5076f1586cf4e81b674e4428e5c3ced409169\n");

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

		cert_t *authkey = cert_read(authkey_path);
		isnt_null(authkey, "Read authkey from test data");
		if (!authkey) BAIL_OUT("failed to read authkey");

		unsigned char unsealed[256 - 64];
		randombytes(unsealed, 256 - 64);
		uint8_t *sealed;
		unsigned long long slen;
		slen = cert_seal(authkey, unsealed, 256 - 64, &sealed);
		is_int(slen, 256, "sealed message is 256 bytes long");

		pdu_t *pdu, *reply;
		pdu = pdu_make("REQUEST", 1, "WrongUser");
		char *pubkey = cert_public_s(authkey);
		pdu_extendf(pdu, "%s", pubkey); free(pubkey);
		pdu_extend( pdu, sealed, 256);  free(sealed);
		pdu_extendf(pdu, "show version");
		pdu_extendf(pdu, ""); /* no filter */

		cert_free(authkey);

		rc = pdu_send_and_free(pdu, sock);
		is_int(rc, 0, "sent REQUEST PDU to meshd thread");

		reply = pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = pdu_string(reply, 0), "ERROR", "REQUEST -> ERROR"); free(s);
		is_string(s = pdu_string(reply, 1), "Authentication failed (pubkey)",
				"Authentication failed (username mismatch)"); free(s);
		is_null(pdu_string(reply, 2), "There is no 3rd frame");
		pdu_free(reply);

		/* send the safe word to terminate the meshd thread */
		pdu = pdu_make("TEST_COMPLETE", 0);
		pdu_send_and_free(pdu, sock);

		void *_;
		pthread_join(tid, &_);
		vzmq_shutdown(sock, 0);
		mesh_server_destroy(server);
		zmq_ctx_destroy(ctx);

		free(authkey_path);
		free(trustdb_path);
	}

	subtest { /* key-based authentication (untrusted key) */
		mkdir("t/tmp", 0777);
		put_file("t/tmp/trustdb", 0644,
		         "cf1fc3e3117ec452e216e8559ac5076f1586cf4e81b674e4428e5c3ced409169 jhacker\n");
		put_file("t/tmp/key.trusted", 0644,
		         "%signing v1\n"
		         "id  rogue\n"
		         "pub 66b5d91fa49d59dbe32e9b3d0f964634805ec01c2174cc0fdc9ace14ca41e83f\n"
		         "sec d4ec9da13262abf0cd98d50b918ae928891b950deed976589a39ef7c154dde8766b5d91fa49d59dbe32e9b3d0f964634805ec01c2174cc0fdc9ace14ca41e83f\n");

		char *trustdb_path = strdup("t/tmp/trustdb");
		char *authkey_path = strdup("t/tmp/key.trusted");

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

		cert_t *authkey = cert_read(authkey_path);
		isnt_null(authkey, "Read authkey from test data");
		if (!authkey) BAIL_OUT("failed to read authkey");

		unsigned char unsealed[256 - 64];
		randombytes(unsealed, 256 - 64);
		uint8_t *sealed;
		unsigned long long slen;
		slen = cert_seal(authkey, unsealed, 256 - 64, &sealed);
		is_int(slen, 256, "sealed message is 256 bytes long");

		pdu_t *pdu, *reply;
		pdu = pdu_make("REQUEST", 1, "rogue");
		char *pubkey = cert_public_s(authkey);
		pdu_extendf(pdu, "%s", pubkey); free(pubkey);
		pdu_extend( pdu, sealed, 256);  free(sealed);
		pdu_extendf(pdu, "show version");
		pdu_extendf(pdu, ""); /* no filter */

		cert_free(authkey);

		rc = pdu_send_and_free(pdu, sock);
		is_int(rc, 0, "sent REQUEST PDU to meshd thread");

		reply = pdu_recv(sock);
		isnt_null(reply, "Got a reply from meshd thread");
		is_string(s = pdu_string(reply, 0), "ERROR", "REQUEST -> ERROR"); free(s);
		is_string(s = pdu_string(reply, 1), "Authentication failed (pubkey)",
				"Authentication failed (untrusted key)"); free(s);
		is_null(pdu_string(reply, 2), "There is no 3rd frame");
		pdu_free(reply);

		/* send the safe word to terminate the meshd thread */
		pdu = pdu_make("TEST_COMPLETE", 0);
		pdu_send_and_free(pdu, sock);

		void *_;
		pthread_join(tid, &_);
		vzmq_shutdown(sock, 0);
		mesh_server_destroy(server);
		zmq_ctx_destroy(ctx);

		free(authkey_path);
		free(trustdb_path);
	}

	done_testing();
}
