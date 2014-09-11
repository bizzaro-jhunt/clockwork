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

#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "mesh.h"
#include "gear/gear.h"
#include "userdb.h"
#include "policy.h"
#include "pendulum.h"
#include "pendulum_funcs.h"

typedef struct {
	cw_list_t   l;

	char       *fqdn;
	char       *status;
	char       *output;
} mesh_result_t;

typedef struct {
	char       *ident;
	uint64_t    serial;

	cw_list_t   results;

	char       *username;
	char       *command;
} mesh_slot_t;

static char * s_string(const char *a, const char *b)
{
	assert(a); assert(b);
	assert(b >= a);

	char *s = cw_alloc(sizeof(char) * (b - a + 1));
	memcpy(s, a, b - a);
	s[b - a] = '\0';
	return s;
}

static const char* s_tokenbound(const char *a)
{
	assert(a);

	const char *b = a;
	char quote = '\0';
	int esc = 0;

	if (*b == '\'' || *b == '"') quote = *b++;

	while (*b) {
		if ( esc)               { esc = 0; b++; continue; }
		if (!esc && *b == '\\') { esc = 1; b++; continue; }

		if ( quote && *b == quote) return ++b; /* include the trailing quote */
		if (!quote && isspace(*b)) return b;
		b++;
	}
	return b;
}

static char *s_unescape(char *buf)
{
	assert(buf);

	char *a, *b;
	a = b = buf;
	char quote = '\0';
	int esc = 0;

	if (*b == '\'' || *b == '"') quote = *b++;

	while (*a && *b) {
		if ( esc)               { *a++ = *b++; esc = 0; continue; }
		if (!esc && *b == '\\') {         b++; esc = 1; continue; }

		if (quote && *b == quote) *a   = '\0';
		else                      *a++ = *b++;
	}
	*a = '\0';
	return buf;
}

static char* s_quote_escape(const char *s)
{
	int n = 2 + 1; /* two quotes and a NULL-terminator */
	const char *a;
	for (a = s; *a; a++) {
		n++;
		if (*a == '"' || *a == '\\')
			n++;
	}

	char *q = cw_alloc(sizeof(char) * n);
	char *b = q;

	*b++ = '"';
	for (a = s; *a; ) {
		if (*a == '"' || *a == '\\')
			*b++ = '\\';
		*b++ = *a++;
	}
	*b++ = '"';
	*b++ = '\0';

	return q;
}

static char * s_qstring(const char *a, const char *b)
{
	char *s = s_string(a, b);
	if (!s) return NULL;
	return s_unescape(s);
}

static void s_mesh_slot_free(void *p)
{
	if (!p) return;
	mesh_slot_t *c = (mesh_slot_t*)p;
	free(c->ident);
	free(c->username);
	free(c->command);

	mesh_result_t *r, *tmp;
	for_each_object_safe(r, tmp, &c->results, l) {
		free(r->fqdn);
		free(r->status);
		free(r->output);
		free(r);
	}

	free(c);
}

static char* s_user_lookup(const char *username)
{
	char *s = NULL;
	struct pwdb *pdb = pwdb_init(SYS_PASSWD);
	struct grdb *gdb = grdb_init(SYS_GROUP);

	if (pdb && gdb)
		s = userdb_lookup(pdb, gdb, username);
	if (!s)
		s = strdup("");

	grdb_free(gdb);
	pwdb_free(pdb);
	return s;
}

static char* s_cmd_code(const char *command)
{
	cmd_t *cmd = cmd_parse(command, COMMAND_LITERAL);
	if (!cmd) return cw_string(";; %s\n", command);

	FILE *io = tmpfile();
	cmd_gencode(cmd, io);
	cmd_destroy(cmd);

	size_t n1 = ftell(io);
	rewind(io);

	char *code = cw_alloc(n1+1);
	size_t n2 = fread(code, 1, n1, io);
	if (n2 < n1) {
		free(code);
		code = cw_string(";; short read error %i/%i\n", n2, n1);
	} else {
		code[n2] = '\0';
	}

	fclose(io);
	return code;
}
/*
     ######  ##     ## ########
    ##    ## ###   ### ##     ##
    ##       #### #### ##     ##
    ##       ## ### ## ##     ##
    ##       ##     ## ##     ##
    ##    ## ##     ## ##     ##
     ######  ##     ## ########

 */

cmd_t* cmd_new(void)
{
	cmd_t *cmd = cw_alloc(sizeof(cmd_t));
	cw_list_init(&cmd->tokens);
	return cmd;
}

void cmd_destroy(cmd_t *cmd)
{
	if (!cmd) return;
	free(cmd->string);

	cmd_token_t *t, *tmp;
	for_each_object_safe(t, tmp, &cmd->tokens, l) {
		free(t->value);
		free(t);
	}
	free(cmd);
}

static int s_cmd_reconstruct(cmd_t *cmd)
{
	assert(cmd);

	struct stringlist *tokens = stringlist_new(NULL);
	cmd_token_t *t;

	for_each_object(t, &cmd->tokens, l) {
		int quoted = 0;
		char *a;
		for (a = t->value; *a; a++) {
			if (isspace(*a) || *a == '"' || *a == '\'' || *a == '\\') {
				quoted = 1;
				break;
			}
		}

		if (quoted) {
			char *s = s_quote_escape(t->value);
			stringlist_add(tokens, s);
			free(s);
		} else {
			stringlist_add(tokens, t->value);
		}
	}

	cmd->string = stringlist_join(tokens, " ");
	stringlist_free(tokens);
	return 0;
}

cmd_t* cmd_parse(const char *s, int pattern)
{
	assert(s);

	cmd_t *cmd = cmd_new();
	cmd_token_t *t;

	/* tokenize */
	const char *a, *b;
	a = b = s;

	for (;;) {
		for (; *a && isspace(*a); a++)
			;
		if (!*a) break;

		t = cw_alloc(sizeof(cmd_token_t));
		t->value = s_string(a, b = s_tokenbound(a));
		s_unescape(t->value);

		t->type = COMMAND_TOKEN_LITERAL;
		if (pattern == COMMAND_PATTERN && strcmp(t->value, "*") == 0)
			t->type = COMMAND_TOKEN_WILDCARD;

		cw_list_push(&cmd->tokens, &t->l);

		a = b;
		if (!*a) break;
	}

	if (cw_list_isempty(&cmd->tokens)) {
		cmd_destroy(cmd);
		return NULL;
	}

	s_cmd_reconstruct(cmd);

	int wildcards = 0;
	for_each_object(t, &cmd->tokens, l) {
		if (wildcards || t->type == COMMAND_TOKEN_WILDCARD)
			wildcards++;
	}
	if (wildcards <= 1) return cmd;

	cmd_destroy(cmd);
	return NULL;
}

cmd_t* cmd_parsev(const char **argv, int pattern)
{
	assert(argv);

	cmd_t *cmd = cmd_new();
	cmd_token_t *t;

	int i;
	for (i = 0; argv[i]; i++) {
		t = cw_alloc(sizeof(cmd_token_t));
		t->value = strdup(argv[i]);

		t->type = COMMAND_TOKEN_LITERAL;
		cw_list_push(&cmd->tokens, &t->l);
	}

	if (cw_list_isempty(&cmd->tokens)) {
		cmd_destroy(cmd);
		return NULL;
	}

	s_cmd_reconstruct(cmd);

	return cmd;
}

int cmd_match(cmd_t *cmd, cmd_t *pat)
{
	assert(cmd);
	assert(pat);

	cw_list_t *c, *c0, *p, *p0;
	cmd_token_t *ct, *pt;
	ct = pt = NULL;
	c0 = &cmd->tokens; c = c0->next;
	p0 = &pat->tokens; p = p0->next;

	for (;;) {
		if (c == c0) {
			/* ended both chains at the same point; match! */
			if (p == p0) return 1;

			/* check to see if our next match would be a wildcard */
			pt = cw_list_object(p, cmd_token_t, l);
			if (pt && pt->type == COMMAND_TOKEN_WILDCARD) return 1;

			return 0;
		}
		if (p == p0) return 0;

		ct = cw_list_object(c, cmd_token_t, l);
		pt = cw_list_object(p, cmd_token_t, l);

		if (pt->type == COMMAND_TOKEN_WILDCARD) return 1;
		if (pt->type == COMMAND_TOKEN_LITERAL
		 && strcmp(pt->value, ct->value) != 0) return 0;

		c = c->next; p = p->next;
	}
	return 0;
}

int cmd_gencode(cmd_t *cmd, FILE *io)
{
	cmd_token_t *tok;
	cw_list_t *l, *end;

	if (cw_list_isempty(&cmd->tokens)) return 0;

	l   = cmd->tokens.next;
	end = &cmd->tokens;
	tok = cw_list_object(l, cmd_token_t, l);

	if (strcmp(tok->value, "show") == 0) {
		if (l->next == end) goto syntax;
		l = l->next;
		tok = cw_list_object(l, cmd_token_t, l);

		if (strcmp(tok->value, "version") == 0) {
			if (l->next != end) goto syntax;
			fprintf(io, "SHOW version\n");
			return 0;
		}

		if (strcmp(tok->value, "acls") == 0) {
			if (l->next == end) {
				fprintf(io, "SHOW acls\n");
				return 0;
			}

			l = l->next;
			tok = cw_list_object(l, cmd_token_t, l);
			if (strcmp(tok->value, "for") == 0) {
				if (l->next == end) goto syntax;
				l = l->next;
				tok = cw_list_object(l, cmd_token_t, l);

				if (l->next != end) goto syntax;
				if (*tok->value == '%') {
					fprintf(io, "SHOW acls :%s\n", tok->value + 1);
					return 0;
				}

				fprintf(io, "SHOW acls %s\n", tok->value);
				return 0;
			}
		}
	}

syntax:
	return 0;
}


/*

       ###     ######  ##
      ## ##   ##    ## ##
     ##   ##  ##       ##
    ##     ## ##       ##
    ######### ##       ##
    ##     ## ##    ## ##
    ##     ##  ######  ########

 */

acl_t* acl_new(void)
{
	acl_t *acl = cw_alloc(sizeof(acl_t));
	cw_list_init(&acl->l);
	return acl;
}

void acl_destroy(acl_t *acl)
{
	if (!acl) return;
	cmd_destroy(acl->pattern);
	cw_list_delete(&acl->l);
	free(acl->target_user);
	free(acl->target_group);
	free(acl);
}

acl_t* acl_parse(const char *s)
{
	const char *a, *b;
	char *disposition, *target, *command, *final = NULL;

	for (a = s; *a && isspace(*a); a++) ;
	for (b = a; *b && !isspace(*b); b++) ;
	disposition = s_string(a, b);

	if (strcmp(disposition, "allow") != 0
	 && strcmp(disposition, "deny")  != 0) {
		free(disposition);
		return NULL;
	}

	for (a = b; *a && isspace(*a); a++) ;
	for (b = a; *b && !isspace(*b); b++) ;
	target = s_string(a, b);

	for (a = b; *a && isspace(*a); a++) ;
	command = s_qstring(a, b = s_tokenbound(a));

	for (a = b; *a && isspace(*a); a++) ;
	for (b = a; *b && !isspace(*b); b++) ;
	if (a && b) final = s_string(a, b);

	acl_t *acl = acl_new();

	if (strcmp(disposition, "allow") == 0) {
		acl->disposition = ACL_ALLOW;
		acl->is_final    = 0;
	} else {
		acl->disposition = ACL_DENY;
		acl->is_final    = 1;
	}
	free(disposition);

	if (*target == '%') {
		acl->target_group = strdup(target+1);
		free(target);
	} else {
		acl->target_user = target;
	}

	if (final && strcmp(final, "final") == 0) acl->is_final = 1;
	free(final);

	acl->pattern = cmd_parse(command, COMMAND_PATTERN);
	free(command);
	if (!acl->pattern) {
		acl_destroy(acl);
		return NULL;
	}

	return acl;
}

char *acl_string(acl_t *a)
{
	assert(a);

	static char *s = NULL;
	s = cw_string("%s %s%s \"%s\"%s",
			a->disposition == ACL_ALLOW ? "allow" : "deny",
			a->target_group ? "%" : "",
			a->target_group ? a->target_group : a->target_user,
			a->pattern->string,
			a->disposition == ACL_ALLOW && a->is_final ? " final" : "");
	return s;
}

int acl_read(cw_list_t *l, const char *path)
{
	assert(l);
	assert(path);

	FILE *io = fopen(path, "r");
	if (!io) return 1;

	int rc = acl_readio(l, io);
	fclose(io);
	return rc;
}

int acl_readio(cw_list_t *l, FILE *io)
{
	assert(l);
	assert(io);

	char *p, buf[8192];
	acl_t *acl;
	while (fgets(buf, 8191, io) != NULL) {
		for (p = buf; *p && isspace(*p); p++)
			;
		if (!*p || *p == '#') continue;

		acl = acl_parse(p);
		if (!acl) return 1;

		cw_list_push(l, &acl->l);
	}
	return 0;
}

int acl_write(cw_list_t *l, const char *path)
{
	assert(l);
	assert(path);

	FILE *io = fopen(path, "w");
	if (!io) return 1;

	int rc = acl_writeio(l, io);
	fclose(io);
	return rc;
}

int acl_writeio(cw_list_t *l, FILE *io)
{
	assert(l);
	assert(io);

	acl_t *a;
	char *s;

	fprintf(io, "# clockwork acl\n");
	for_each_object(a, l, l) {
		s = acl_string(a);
		fprintf(io, "%s\n", s);
		free(s);
	}

	return 0;
}


int acl_gencode(acl_t *a, FILE *io)
{
	assert(a);
	assert(io);

	fprintf(io, "ACL \"%s %s%s \\\"%s\\\"%s\"\n",
			a->disposition == ACL_ALLOW ? "allow" : "deny",
			a->target_group ? "%" : "",
			a->target_group ? a->target_group : a->target_user,
			a->pattern->string,
			a->disposition == ACL_ALLOW && a->is_final ? " final" : "");
	return 0;
}

int acl_match(acl_t *acl, const char *id, cmd_t *cmd)
{
	assert(acl);
	assert(id);

	const char *a, *b;
	char *tmp;
	int applies = 0;

	if (acl->target_user) {
		a = id; b = strchr(id, ':');
		tmp = b ? s_string(a, b) : strdup(a);

		applies = (strcmp(tmp, acl->target_user) == 0);
		free(tmp);
		if (!applies) return 0;

	} else if (acl->target_group) {
		a = strchr(id, ':');
		while (a) {
			b = strchr(++a, ':');
			tmp = b ? s_string(a, b) : strdup(a);

			applies = (strcmp(tmp, acl->target_group) == 0);
			free(tmp);
			if (applies) break;
			a = b;
		}
		if (!applies) return 0;
	}

	if (!cmd) return 1;
	return cmd_match(cmd, acl->pattern);
}

int acl_check(cw_list_t *all, const char *id, cmd_t *cmd)
{
	int disposition = ACL_NEUTRAL;
	acl_t *acl;
	for_each_object(acl, all, l) {
		if (!acl_match(acl, id, cmd)) continue;
		disposition = acl->disposition;
		if (acl->is_final) break;
	}
	return disposition;
}


/*

    ######## #### ##       ######## ######## ########
    ##        ##  ##          ##    ##       ##     ##
    ##        ##  ##          ##    ##       ##     ##
    ######    ##  ##          ##    ######   ########
    ##        ##  ##          ##    ##       ##   ##
    ##        ##  ##          ##    ##       ##    ##
    ##       #### ########    ##    ######## ##     ##

 */

filter_t* filter_new(void)
{
	filter_t *f = cw_alloc(sizeof(filter_t));
	return f;
}

void filter_destroy(filter_t *f)
{
	if (!f) return;
	free(f->fact);
	free(f->literal);
	pcre_free(f->regex);
	free(f);
}

filter_t* filter_parse(const char *s)
{
	filter_t *f = filter_new();

	const char *a, *b;
	for (b = s; *b && isspace(*b); b++) ;
	for (a = b; *b && !isspace(*b) && *b != '=' && *b != '!'; b++) ;
	f->fact = s_string(a, b);

	for (; *b && isspace(*b); b++) ;
	f->match = 1;
	if (*b == '!') {
		f->match = 0;
		b++;
	}
	b++; // '='

	for (; *b && isspace(*b); b++)
		;

	for (a = b; *b && *b != '\n' && *b != '\r'; b++)
		;
	if (*a == '/' && *(b-1) == '/') {
		a++; b--;
		char *pat = s_string(a, b);

		const char *e_string;
		int e_offset;
		f->regex = pcre_compile(pat, PCRE_CASELESS, &e_string, &e_offset, NULL);
		free(pat);

		/* FIXME: what do we do with the error string?? */
	} else {
		f->literal = s_string(a, b);
	}

	return f;
}

int filter_parseall(cw_list_t *list, const char *_s)
{
	assert(list);
	assert(_s);

	filter_t *f;
	char *a, *b, *s;
	a = s = strdup(_s);
	while ( (b = strchr(a, '\n')) != NULL) {
		*b = '\0';
		if ((f = filter_parse(a)) != NULL)
			cw_list_push(list, &f->l);
		a = ++b;
	}

	free(s);
	return 0;
}

int filter_match(filter_t *f, cw_hash_t *facts)
{
	const char *v = cw_hash_get(facts, f->fact);
	if (!v) return 0;

	if (f->literal &&  f->match) return strcmp(v, f->literal) == 0;
	if (f->literal && !f->match) return strcmp(v, f->literal) != 0;

	if (f->regex) {
		int rc = pcre_exec(f->regex, NULL, v, strlen(v), 0, 0, NULL, 0);
		return f->match ? rc >= 0 : rc < 0;
	}
	return 0;
}

int filter_matchall(cw_list_t *all, cw_hash_t *facts)
{
	filter_t *f;
	for_each_object(f, all, l)
		if (!filter_match(f, facts))
			return 0;
	return 1;
}

/*

    ##     ## ########  ######  ##     ##
    ###   ### ##       ##    ## ##     ##
    #### #### ##       ##       ##     ##
    ## ### ## ######    ######  #########
    ##     ## ##             ## ##     ##
    ##     ## ##       ##    ## ##     ##
    ##     ## ########  ######  ##     ##

 */

mesh_server_t* mesh_server_new(void *zmq)
{
	cw_srand();

	mesh_server_t *s = cw_alloc(sizeof(mesh_server_t));
	s->serial = ((uint64_t)(rand() * 4294967295.0 / RAND_MAX) << 31) + 1;

	if (!zmq) {
		s->zmq_auto = 1;
		s->zmq = zmq_ctx_new();
	} else {
		s->zmq = zmq;
	}

	s->pam_service = strdup("clockwork");
	s->_safe_word = NULL;

	cw_list_init(&s->acl);

	s->slots = cw_cache_new(128, 600);
	s->slots->destroy_f = s_mesh_slot_free;

	s->cert = cw_cert_generate();
	return s;
}

int mesh_server_setopt(mesh_server_t *s, int opt, void *data, size_t len)
{
	assert(s);
	errno = EINVAL;

	acl_t *acl, *acl_tmp;

	switch (opt) {
	case MESH_SERVER_SERIAL:
		if (len != sizeof(uint64_t)) return -1;
		s->serial = *(uint64_t*)data;
		return 0;

	case MESH_SERVER_PAM_SERVICE:
		free(s->pam_service);
		if (len == 0) {
			s->pam_service = NULL;
		} else {
			s->pam_service = cw_alloc((len + 1) * sizeof(char));
			memcpy(s->pam_service, data, len);
		}
		return 0;

	case MESH_SERVER_CERTIFICATE:
		if (len != sizeof(cw_cert_t)) return -1;
		free(s->cert->ident);
		s->cert->ident = ((cw_cert_t *)data)->ident
			? strdup(((cw_cert_t *)data)->ident)
			: NULL;
		s->cert->pubkey = ((cw_cert_t *)data)->pubkey;
		memcpy(s->cert->pubkey_bin, ((cw_cert_t *)data)->pubkey_bin, 32);
		memcpy(s->cert->pubkey_b16, ((cw_cert_t *)data)->pubkey_b16, 65);
		s->cert->seckey = ((cw_cert_t *)data)->seckey;
		memcpy(s->cert->seckey_bin, ((cw_cert_t *)data)->seckey_bin, 32);
		memcpy(s->cert->seckey_b16, ((cw_cert_t *)data)->seckey_b16, 65);
		return 0;

	case MESH_SERVER_SAFE_WORD:
		free(s->_safe_word);
		if (len == 0) {
			s->_safe_word = NULL;
		}  else {
			s->_safe_word = cw_alloc((len + 1) * sizeof(char));
			memcpy(s->_safe_word, data, len);
		}
		return 0;

	case MESH_SERVER_CACHE_SIZE:
		if (len != sizeof(size_t)) return -1;
		return cw_cache_tune(&s->slots, *(size_t*)data, 0);

	case MESH_SERVER_CACHE_LIFE:
		if (len != sizeof(int32_t)) return -1;
		return cw_cache_tune(&s->slots, 0, *(int32_t*)data);

	case MESH_SERVER_GLOBAL_ACL:
		for_each_object_safe(acl, acl_tmp, &s->acl, l)
			acl_destroy(acl);
		acl_read(&s->acl, (const char*)data);
		/* FIXME: need to differentiate ENOENT from bad ACL */
		return 0;
	}

	errno = ENOTSUP;
	return -1;
}

int mesh_server_bind_control(mesh_server_t *s, const char *endpoint)
{
	assert(s);
	assert(endpoint);

	s->control = zmq_socket(s->zmq, ZMQ_ROUTER);
	if (!s->control) return -1;

	if (s->cert) {
		int rc, optval = 1;
		rc = zmq_setsockopt(s->control, ZMQ_CURVE_SERVER, &optval, sizeof(optval));
		assert(rc == 0);

		rc = zmq_setsockopt(s->control, ZMQ_CURVE_SECRETKEY, cw_cert_secret(s->cert), 32);
		assert(rc == 0);
	}

	return zmq_bind(s->control, endpoint);
}

int mesh_server_bind_broadcast(mesh_server_t *s, const char *endpoint)
{
	assert(s);
	assert(endpoint);

	s->broadcast = zmq_socket(s->zmq, ZMQ_PUB);
	if (!s->broadcast) return -1;

	if (s->cert) {
		int rc, optval = 1;
		rc = zmq_setsockopt(s->broadcast, ZMQ_CURVE_SERVER, &optval, sizeof(optval));
		assert(rc == 0);

		rc = zmq_setsockopt(s->broadcast, ZMQ_CURVE_SECRETKEY, cw_cert_secret(s->cert), 32);
		assert(rc == 0);
	}

	return zmq_bind(s->broadcast, endpoint);
}

int mesh_server_run(mesh_server_t *s)
{
	int rc;
	s->zap = cw_zap_startup(s->zmq, NULL);

	cw_reactor_t *r = cw_reactor_new();
	if (!r) return -1;

	rc = cw_reactor_add(r, s->control, mesh_server_reactor, s);
	if (rc != 0) return rc;

	rc = cw_reactor_loop(r);
	if (rc != 0) return rc;

	cw_reactor_destroy(r);
	cw_zap_shutdown(s->zap);
	return 0;
}

void mesh_server_destroy(mesh_server_t *s)
{
	if (!s) return;

	cw_zmq_shutdown(s->control, 500);
	cw_zmq_shutdown(s->broadcast, 0);

	if (s->zmq_auto)
		zmq_ctx_destroy(s->zmq);

	free(s->pam_service);
	free(s->_safe_word);

	cw_cert_destroy(s->cert);

	acl_t *acl, *acl_tmp;
	for_each_object_safe(acl, acl_tmp, &s->acl, l)
		acl_destroy(acl);

	cw_cache_purge(s->slots, 1);
	cw_cache_free(s->slots);

	free(s);
}

int mesh_server_reactor(void *sock, cw_pdu_t *pdu, void *data)
{
	cw_pdu_t *reply;
	mesh_server_t *server = (mesh_server_t*)data;

	cw_log(LOG_DEBUG, "Inbound [%s] packet from %s", pdu->type, pdu->client);
	if (strcmp(pdu->type, "RESULT") == 0) {
		char *serial = cw_pdu_text(pdu, 1);
		mesh_slot_t *client = cw_cache_get(server->slots, serial);
		free(serial);

		if (client) {
			mesh_result_t *r = cw_alloc(sizeof(mesh_result_t));
			cw_list_init(&r->l);
			r->fqdn   = cw_pdu_text(pdu, 2);
			r->status = cw_pdu_text(pdu, 3);
			r->output = cw_pdu_text(pdu, 4);

			cw_list_push(&client->results, &r->l);
		}
	}

	if (strcmp(pdu->type, "OPTOUT") == 0) {
		char *serial = cw_pdu_text(pdu, 1);
		mesh_slot_t *client = cw_cache_get(server->slots, serial);
		free(serial);

		if (client) {
			mesh_result_t *r = cw_alloc(sizeof(mesh_result_t));
			cw_list_init(&r->l);
			r->fqdn = cw_pdu_text(pdu, 2);

			cw_list_push(&client->results, &r->l);
		}
	}

	if (strcmp(pdu->type, "REQUEST") == 0) {
		cw_cache_purge(server->slots, 0);
		char *username = cw_pdu_text(pdu, 1),
		     *password = cw_pdu_text(pdu, 2),
		     *command  = cw_pdu_text(pdu, 3),
		     *filters  = cw_pdu_text(pdu, 4),
		     *creds    = NULL,
		     *code     = NULL;

		mesh_slot_t *client = cw_alloc(sizeof(mesh_slot_t));
		client->ident    = strdup(pdu->client);
		client->serial   = server->serial++;
		client->username = username;
		client->command  = command;
		cw_list_init(&client->results);

		char *serial = cw_string("%lx", client->serial);

		if (!cw_cache_set(server->slots, serial, client)) {
			reply = cw_pdu_make(pdu->src, 2, "ERROR", "Too many client connections; try again later");
			cw_pdu_send(sock, reply);
			cw_pdu_destroy(reply);
			goto REQUEST_exit;
		}

		creds = s_user_lookup(username); assert(creds);
		code  = s_cmd_code(command);     assert(code);

		if (server->pam_service) {
			cw_log(LOG_DEBUG, "authenticating as %s", username);
			int rc = cw_authenticate(server->pam_service, username, password);
			free(password);

			if (rc != 0) {
				reply = cw_pdu_make(pdu->src, 2, "ERROR", "Authentication failed");
				cw_pdu_send(sock, reply);
				cw_pdu_destroy(reply);
				goto REQUEST_exit;
			}
		} else {
			free(password);
		}

		cmd_t *cmd = cmd_parse(command, COMMAND_LITERAL);
		if (!cmd) {
			cw_log(LOG_DEBUG, "failed to parse command `%s`", command);
			reply = cw_pdu_make(pdu->src, 2, "ERROR", "Failed to parse command");
			cw_pdu_send(sock, reply);
			cw_pdu_destroy(reply);
			goto REQUEST_exit;
		}

		cw_log(LOG_DEBUG, "checking global ACL for %s `%s`", creds, command);
		int rc = acl_check(&server->acl, creds, cmd);
		cmd_destroy(cmd);
		if (rc == ACL_DENY) {
			cw_log(LOG_DEBUG, "access to %s `%s` denied by global ACL", creds, command);
			reply = cw_pdu_make(pdu->src, 2, "ERROR", "Permission denied");
			cw_pdu_send(sock, reply);
			cw_pdu_destroy(reply);
			goto REQUEST_exit;
		}

		cw_pdu_t *blast = cw_pdu_make(NULL, 6, "COMMAND", serial, creds, command, code, filters);
		cw_pdu_send(server->broadcast, blast);
		cw_pdu_destroy(blast);

		reply = cw_pdu_make(pdu->src, 2, "SUBMITTED", serial);
		cw_pdu_send(sock, reply);
		cw_pdu_destroy(reply);

REQUEST_exit:
		free(serial);
		free(creds);
		free(code);
		free(filters);
		return CW_REACTOR_CONTINUE;
	}

	if (strcmp(pdu->type, "CHECK") == 0) {
		mesh_result_t *r, *r_tmp;
		char *serial = cw_pdu_text(pdu, 1);
		mesh_slot_t *client = cw_cache_get(server->slots, serial);
		free(serial);

		if (client) {
			for_each_object_safe(r, r_tmp, &client->results, l) {
				reply = (r->status && r->output)
					? cw_pdu_make(pdu->src, 4, "RESULT", r->fqdn, r->status, r->output)
					: cw_pdu_make(pdu->src, 2, "OPTOUT", r->fqdn);

				cw_pdu_send(sock, reply);
				cw_pdu_destroy(reply);
				cw_list_delete(&r->l);
				free(r->fqdn);
				free(r->status);
				free(r->output);
				free(r);
			}

			reply = cw_pdu_make(pdu->src, 1, "DONE");
		} else {
			reply = cw_pdu_make(pdu->src, 2, "ERROR", "not a client");
		}
		cw_pdu_send(sock, reply);
		cw_pdu_destroy(reply);
	}

	if (server->_safe_word && strcmp(pdu->type, server->_safe_word) == 0)
		return 0;

	return CW_REACTOR_CONTINUE;
}

mesh_client_t* mesh_client_new(void)
{
	mesh_client_t *c = cw_alloc(sizeof(mesh_client_t));
	c->acl_default = ACL_DENY;
	return c;
}

int mesh_client_setopt(mesh_client_t *c, int opt, void *data, size_t len)
{
	assert(c);
	errno = EINVAL;

	switch (opt) {
	case MESH_CLIENT_FQDN:
		free(c->fqdn);
		if (len == 0) {
			c->fqdn = NULL;
		} else {
			c->fqdn = cw_alloc((len + 1) * sizeof(char));
			memcpy(c->fqdn, data, len);
		}
		return 0;

	case MESH_CLIENT_GATHERERS:
		free(c->gatherers);
		if (len == 0) {
			c->gatherers = NULL;
		} else {
			c->gatherers = cw_alloc((len + 1) * sizeof(char));
			memcpy(c->gatherers, data, len);
		}
		return 0;

	case MESH_CLIENT_FACTS:
		if (len != sizeof(cw_hash_t*)) return -1;
		c->facts = (cw_hash_t*)data;
		return 0;

	case MESH_CLIENT_ACL:
		if (len != sizeof(cw_list_t*)) return -1;
		c->acl = (cw_list_t*)data;
		return 0;

	case MESH_CLIENT_ACL_DEFAULT:
		if (len != sizeof(int)) return -1;
		c->acl_default = *(int*)data;
		return 0;
	}

	errno = ENOTSUP;
	return -1;
}

void mesh_client_destroy(mesh_client_t *c)
{
	if (!c) return;

	free(c->fqdn);
	free(c->gatherers);
	free(c);
}

int mesh_client_handle(mesh_client_t *c, void *sock, cw_pdu_t *pdu)
{
	assert(pdu);
	cw_pdu_t *reply;

	cw_log(LOG_INFO, "inbound %s packet", pdu->type);
	if (strcmp(pdu->type, "COMMAND") == 0) {
		char *serial  = cw_pdu_text(pdu, 1),
		     *creds   = cw_pdu_text(pdu, 2),
		     *command = cw_pdu_text(pdu, 3),
		     *code    = cw_pdu_text(pdu, 4),
		     *filters = cw_pdu_text(pdu, 5);

		assert(serial);
		assert(creds);
		assert(command);
		assert(code);
		assert(filters);

		cmd_t *cmd = cmd_parse(command, COMMAND_LITERAL);
		assert(cmd);

		cw_log(LOG_DEBUG, "inbound COMMAND `%s` on behalf of %s", command, creds);
		int disposition = acl_check(c->acl, creds, cmd);
		cmd_destroy(cmd);

		if (disposition == ACL_NEUTRAL)
			disposition = c->acl_default;

		if (disposition == ACL_DENY) {
			cw_log(LOG_INFO, "denied `%s` to %s per ACL", command, creds);
			goto bail;
		}

		if (!c->facts) {
			c->facts = cw_alloc(sizeof(cw_hash_t));
			cw_log(LOG_DEBUG, "no cached facts; gathering now");
			fact_gather(c->gatherers, c->facts);
		}

		LIST(filter);
		filter_parseall(&filter, filters);
		if (!filter_matchall(&filter, c->facts)) {
			cw_log(LOG_INFO, "opting out of `%s` command from %s, per filters",
					command, creds);
			reply = cw_pdu_make(NULL, 3, "OPTOUT", serial, c->fqdn);
			cw_pdu_send(sock, reply);
			cw_pdu_destroy(reply);
			goto bail;
		}

		pn_machine m;
		pn_init(&m);
		pendulum_init(&m, NULL);
		m.output = tmpfile();

		pn_parse_s(&m, code);
		pn_run(&m);

		rewind(m.output);
		char output[8192] = {0};
		if (!fgets(output, 8192, m.output))
			output[0] = '\0';
		fclose(m.output);

		pendulum_destroy(&m);
		pn_destroy(&m);

		reply = cw_pdu_make(NULL, 5, "RESULT", serial, c->fqdn, "0", output);
		cw_pdu_send(sock, reply);
		cw_pdu_destroy(reply);

bail:
		free(serial);
		free(creds);
		free(command);
		free(code);
		free(filters);

	} else {
		cw_log(LOG_DEBUG, "unrecognized PDU: '%s'", pdu->type);
		/* ignore */
	}

	return 0;
}
