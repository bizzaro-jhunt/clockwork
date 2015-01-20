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
#include "authdb.h"
#include "policy.h"
#include "vm.h"

typedef struct {
	list_t      l;

	char       *fqdn;
	char       *uuid;
	char       *status;
	char       *output;
} mesh_result_t;

typedef struct {
	char       *ident;
	uint64_t    serial;

	list_t      results;

	char       *username;
	char       *command;
} mesh_slot_t;

static char * s_string(const char *a, const char *b)
{
	assert(a); assert(b);
	assert(b >= a);

	char *s = vmalloc(b - a + 1);
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

	char *q = vmalloc(n);
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
		free(r->uuid);
		free(r->status);
		free(r->output);
		free(r);
	}

	free(c);
}

static char* s_user_lookup(const char *username)
{
	char *s = NULL;
	authdb_t *db = authdb_read(AUTHDB_ROOT, AUTHDB_ALL);
	if (!db) return strdup("");

	if (db) s = authdb_creds(db, username);
	if (!s) s = strdup("");

	authdb_close(db);
	return s;
}

static int s_cmd_compile(const char *command, byte_t **code, size_t *len)
{
	cmd_t *cmd = cmd_parse(command, COMMAND_LITERAL);
	if (!cmd) return -1;

	FILE *io = tmpfile();
	cmd_gencode(cmd, io);
	cmd_destroy(cmd);
	rewind(io);

	asm_t *pna = asm_new();

	int rc;
	rc = asm_setopt(pna, PNASM_OPT_INIO, io, sizeof(io));
	if (rc != 0) goto bail;

	rc = asm_setopt(pna, PNASM_OPT_INFILE, "mesh", 4);
	if (rc != 0) goto bail;

	/* FIXME: pendulum.inc in mesh configs! */

	int strip = 1;
	rc = asm_setopt(pna, PNASM_OPT_STRIPPED, &strip, sizeof(strip));
	if (rc != 0) goto bail;

	rc = asm_compile(pna);
	if (rc != 0) goto bail;

	*code = vmalloc(pna->size);
	if (!*code) goto bail;

	memcpy(*code, pna->code, pna->size);
	*len = pna->size;
	asm_free(pna);
	return 0;

bail:
	free(pna);
	return 1;
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
	cmd_t *cmd = vmalloc(sizeof(cmd_t));
	list_init(&cmd->tokens);
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

	strings_t *tokens = strings_new(NULL);
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
			strings_add(tokens, s);
			free(s);
		} else {
			strings_add(tokens, t->value);
		}
	}

	cmd->string = strings_join(tokens, " ");
	strings_free(tokens);
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

		t = vmalloc(sizeof(cmd_token_t));
		t->value = s_string(a, b = s_tokenbound(a));
		s_unescape(t->value);

		t->type = COMMAND_TOKEN_LITERAL;
		if (pattern == COMMAND_PATTERN && strcmp(t->value, "*") == 0)
			t->type = COMMAND_TOKEN_WILDCARD;

		list_push(&cmd->tokens, &t->l);

		a = b;
		if (!*a) break;
	}

	if (list_isempty(&cmd->tokens)) {
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
		t = vmalloc(sizeof(cmd_token_t));
		t->value = strdup(argv[i]);

		t->type = COMMAND_TOKEN_LITERAL;
		list_push(&cmd->tokens, &t->l);
	}

	if (list_isempty(&cmd->tokens)) {
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

	list_t *c, *c0, *p, *p0;
	cmd_token_t *ct, *pt;
	ct = pt = NULL;
	c0 = &cmd->tokens; c = c0->next;
	p0 = &pat->tokens; p = p0->next;

	for (;;) {
		if (c == c0) {
			/* ended both chains at the same point; match! */
			if (p == p0) return 1;

			/* check to see if our next match would be a wildcard */
			pt = list_object(p, cmd_token_t, l);
			if (pt && pt->type == COMMAND_TOKEN_WILDCARD) return 1;

			return 0;
		}
		if (p == p0) return 0;

		ct = list_object(c, cmd_token_t, l);
		pt = list_object(p, cmd_token_t, l);

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
	list_t *l, *end;

	if (list_isempty(&cmd->tokens)) return 0;

	l   = cmd->tokens.next;
	end = &cmd->tokens;
	tok = list_object(l, cmd_token_t, l);

	if (strcmp(tok->value, "show") == 0) {
		if (l->next == end) goto syntax;
		l = l->next;
		tok = list_object(l, cmd_token_t, l);

		if (strcmp(tok->value, "version") == 0) {
			if (l->next != end) goto syntax;
			fprintf(io, "fn main\n"
			            "  property \"version\" %%a\n"
			            "  print %%a\n");
			return 0;
		}

		if (strcmp(tok->value, "acls") == 0) {
			if (l->next == end) {
				fprintf(io, "fn main\n"
				            "  show.acls\n");
				return 0;
			}

			l = l->next;
			tok = list_object(l, cmd_token_t, l);
			if (strcmp(tok->value, "for") == 0) {
				if (l->next == end) goto syntax;
				l = l->next;
				tok = list_object(l, cmd_token_t, l);

				if (l->next != end) goto syntax;
				if (*tok->value == '%') {
					fprintf(io, "fn main\n"
					            "  show.acl \":%s\"\n", tok->value + 1);
					return 0;
				}

				fprintf(io, "fn main\n"
				            "  show.acl \"%s\"\n", tok->value);
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
	acl_t *acl = vmalloc(sizeof(acl_t));
	list_init(&acl->l);
	return acl;
}

void acl_destroy(acl_t *acl)
{
	if (!acl) return;
	cmd_destroy(acl->pattern);
	list_delete(&acl->l);
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
	s = string("%s %s%s \"%s\"%s",
			a->disposition == ACL_ALLOW ? "allow" : "deny",
			a->target_group ? "%" : "",
			a->target_group ? a->target_group : a->target_user,
			a->pattern->string,
			a->disposition == ACL_ALLOW && a->is_final ? " final" : "");
	return s;
}

int acl_read(list_t *l, const char *path)
{
	assert(l);
	assert(path);

	FILE *io = fopen(path, "r");
	if (!io) return 1;

	int rc = acl_readio(l, io);
	fclose(io);
	return rc;
}

int acl_readio(list_t *l, FILE *io)
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

		list_push(l, &acl->l);
	}
	return 0;
}

int acl_write(list_t *l, const char *path)
{
	assert(l);
	assert(path);

	FILE *io = fopen(path, "w");
	if (!io) return 1;

	int rc = acl_writeio(l, io);
	fclose(io);
	return rc;
}

int acl_writeio(list_t *l, FILE *io)
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

	fprintf(io, "acl \"%s %s%s \\\"%s\\\"%s\"\n",
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

int acl_check(list_t *all, const char *id, cmd_t *cmd)
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
	filter_t *f = vmalloc(sizeof(filter_t));
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

		const char *e_string = NULL; int e_offset;
		f->regex = pcre_compile(pat, PCRE_CASELESS, &e_string, &e_offset, NULL);
		free(pat);

		if (e_string) {
			logger(LOG_ERR, "failed to parse regular expression for acl: %s", e_string);
			f->regex = NULL;
		}
	} else {
		f->literal = s_string(a, b);
	}

	return f;
}

int filter_parseall(list_t *list, const char *_s)
{
	assert(list);
	assert(_s);

	filter_t *f;
	char *a, *b, *s;
	a = s = strdup(_s);
	while ( (b = strchr(a, '\n')) != NULL) {
		*b = '\0';
		if ((f = filter_parse(a)) != NULL)
			list_push(list, &f->l);
		a = ++b;
	}

	free(s);
	return 0;
}

int filter_match(filter_t *f, hash_t *facts)
{
	const char *v = hash_get(facts, f->fact);
	if (!v) return 0;

	if (f->literal &&  f->match) return strcmp(v, f->literal) == 0;
	if (f->literal && !f->match) return strcmp(v, f->literal) != 0;

	if (f->regex) {
		int rc = pcre_exec(f->regex, NULL, v, strlen(v), 0, 0, NULL, 0);
		return f->match ? rc >= 0 : rc < 0;
	}
	return 0;
}

int filter_matchall(list_t *all, hash_t *facts)
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
	seed_randomness();

	mesh_server_t *s = vmalloc(sizeof(mesh_server_t));
	s->serial = ((uint64_t)(rand() * 4294967295.0 / RAND_MAX) << 31) + 1;

	if (!zmq) {
		s->zmq_auto = 1;
		s->zmq = zmq_ctx_new();
	} else {
		s->zmq = zmq;
	}

	s->pam_service = strdup("clockwork");
	s->_safe_word = NULL;

	list_init(&s->acl);

	s->slots = cache_new(128, 600);
	cache_setopt(s->slots, VIGOR_CACHE_DESTRUCTOR, s_mesh_slot_free);

	s->cert = cert_generate(VIGOR_CERT_ENCRYPTION);
	return s;
}

int mesh_server_setopt(mesh_server_t *s, int opt, const void *data, size_t len)
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
			s->pam_service = vmalloc(len + 1);
			memcpy(s->pam_service, data, len);
		}
		return 0;

	case MESH_SERVER_CERTIFICATE:
		if (len != sizeof(cert_t)) return -1;
		free(s->cert->ident);
		s->cert->ident = ((cert_t *)data)->ident
			? strdup(((cert_t *)data)->ident)
			: NULL;
		s->cert->pubkey = ((cert_t *)data)->pubkey;
		memcpy(s->cert->pubkey_bin, ((cert_t *)data)->pubkey_bin, 32);
		memcpy(s->cert->pubkey_b16, ((cert_t *)data)->pubkey_b16, 65);
		s->cert->seckey = ((cert_t *)data)->seckey;
		memcpy(s->cert->seckey_bin, ((cert_t *)data)->seckey_bin, 32);
		memcpy(s->cert->seckey_b16, ((cert_t *)data)->seckey_b16, 65);
		return 0;

	case MESH_SERVER_SAFE_WORD:
		free(s->_safe_word);
		if (len == 0) {
			s->_safe_word = NULL;
		}  else {
			s->_safe_word = vmalloc(len + 1);
			memcpy(s->_safe_word, data, len);
		}
		return 0;

	case MESH_SERVER_CACHE_SIZE:
		if (len != sizeof(size_t)) return -1;
		return cache_resize(&s->slots, *(size_t*)data);

	case MESH_SERVER_CACHE_LIFE:
		if (len != sizeof(int32_t)) return -1;
		return cache_setopt(s->slots, VIGOR_CACHE_EXPIRY, data);

	case MESH_SERVER_GLOBAL_ACL:
		for_each_object_safe(acl, acl_tmp, &s->acl, l)
			acl_destroy(acl);
		acl_read(&s->acl, (const char*)data);
		/* FIXME: need to differentiate ENOENT from bad ACL */
		return 0;

	case MESH_SERVER_TRUSTDB:
		trustdb_free(s->trustdb);
		s->trustdb = trustdb_read((const char*)data);
		if (!s->trustdb)
			s->trustdb = trustdb_new();
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
		if (rc != 0) {
			logger(LOG_CRIT, "Failed to set ZMQ_CURVE_SERVER option on control socket: %s",
				zmq_strerror(errno));
			if (errno == EINVAL) {
				int maj, min, rev;
				zmq_version(&maj, &min, &rev);
				logger(LOG_CRIT, "Perhaps the local libzmq (%u.%u.%u) doesn't support CURVE security?",
					maj, min, rev);
			}
			exit(4);
		}

		rc = zmq_setsockopt(s->control, ZMQ_CURVE_SECRETKEY, cert_secret(s->cert), 32);
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

		rc = zmq_setsockopt(s->broadcast, ZMQ_CURVE_SECRETKEY, cert_secret(s->cert), 32);
		assert(rc == 0);
	}

	return zmq_bind(s->broadcast, endpoint);
}

int mesh_server_run(mesh_server_t *s)
{
	int rc;
	s->zap = zap_startup(s->zmq, NULL);

	reactor_t *r = reactor_new();
	if (!r) return -1;

	rc = reactor_set(r, s->control, mesh_server_reactor, s);
	if (rc != 0) return rc;

	rc = reactor_go(r);
	if (rc != 0) return rc;

	reactor_free(r);
	zap_shutdown(s->zap);
	return 0;
}

void mesh_server_destroy(mesh_server_t *s)
{
	if (!s) return;

	vzmq_shutdown(s->control, 500);
	vzmq_shutdown(s->broadcast, 0);

	if (s->zmq_auto)
		zmq_ctx_destroy(s->zmq);

	free(s->pam_service);
	free(s->_safe_word);

	cert_free(s->cert);
	trustdb_free(s->trustdb);

	acl_t *acl, *acl_tmp;
	for_each_object_safe(acl, acl_tmp, &s->acl, l)
		acl_destroy(acl);

	cache_purge(s->slots, 1);
	cache_free(s->slots);

	free(s);
}

int mesh_server_reactor(void *sock, pdu_t *pdu, void *data)
{
	pdu_t *reply;
	mesh_server_t *server = (mesh_server_t*)data;

	logger(LOG_DEBUG, "Inbound [%s] packet from %s", pdu_type(pdu), pdu_peer(pdu));
	if (strcmp(pdu_type(pdu), "RESULT") == 0) {
		char *serial = pdu_string(pdu, 1);
		mesh_slot_t *client = cache_get(server->slots, serial);
		free(serial);

		if (client) {
			mesh_result_t *r = vmalloc(sizeof(mesh_result_t));
			list_init(&r->l);
			r->fqdn   = pdu_string(pdu, 2);
			r->uuid   = pdu_string(pdu, 3);
			r->status = pdu_string(pdu, 4);
			r->output = pdu_string(pdu, 5);

			list_push(&client->results, &r->l);
		}
	}

	if (strcmp(pdu_type(pdu), "OPTOUT") == 0) {
		char *serial = pdu_string(pdu, 1);
		mesh_slot_t *client = cache_get(server->slots, serial);
		free(serial);

		if (client) {
			mesh_result_t *r = vmalloc(sizeof(mesh_result_t));
			list_init(&r->l);
			r->fqdn = pdu_string(pdu, 2);
			r->uuid = pdu_string(pdu, 3);

			list_push(&client->results, &r->l);
		}
	}

	if (strcmp(pdu_type(pdu), "REQUEST") == 0) {
		cache_purge(server->slots, 0);

		size_t secret_len;
		char *username = pdu_string(pdu, 1),
		     *pubkey   = pdu_string(pdu, 2),
		     *secret   = (char*)pdu_segment(pdu, 3, &secret_len),
		     *command  = pdu_string(pdu, 4),
		     *filters  = pdu_string(pdu, 5),
		     *creds    = NULL;

		byte_t *code;
		size_t len;

		mesh_slot_t *client = vmalloc(sizeof(mesh_slot_t));
		client->ident    = strdup(pdu_peer(pdu));
		client->serial   = server->serial++;
		client->username = username;
		client->command  = command;
		list_init(&client->results);

		char *serial = string("%lx", client->serial);

		if (!cache_set(server->slots, serial, client)) {
			reply = pdu_reply(pdu, "ERROR", 1, "Too many client connections; try again later");
			pdu_send_and_free(reply, sock);
			goto REQUEST_exit;
		}

		creds = s_user_lookup(username);
		assert(creds);
		int rc = s_cmd_compile(command, &code, &len);
		assert(rc == 0);

		if (strlen(pubkey) > 0) {
			logger(LOG_INFO, "authenticating as %s using public key %s", username, pubkey);
			cert_t *cert = cert_make(VIGOR_CERT_SIGNING, pubkey, NULL);

			if (!cert_sealed(cert, secret, secret_len)
			 || trustdb_verify(server->trustdb, cert, username) != 0) {

				reply = pdu_reply(pdu, "ERROR", 1, "Authentication failed (pubkey)");
				cert_free(cert);
				pdu_send_and_free(reply, sock);
				goto REQUEST_exit;
			}
			cert_free(cert);

		} else if (server->pam_service) {
			logger(LOG_DEBUG, "authenticating as %s using supplied password", username);
			if (cw_authenticate(server->pam_service, username, secret) != 0) {
				reply = pdu_reply(pdu, "ERROR", 1, "Authentication failed (password)");
				pdu_send_and_free(reply, sock);
				goto REQUEST_exit;
			}

		} else {
			logger(LOG_INFO, "authentication disabled; allowing");
		}

		cmd_t *cmd = cmd_parse(command, COMMAND_LITERAL);
		if (!cmd) {
			logger(LOG_DEBUG, "failed to parse command `%s`", command);
			reply = pdu_reply(pdu, "ERROR", 1, "Failed to parse command");
			pdu_send_and_free(reply, sock);
			goto REQUEST_exit;
		}

		logger(LOG_DEBUG, "checking global ACL for %s `%s`", creds, command);
		rc = acl_check(&server->acl, creds, cmd);
		cmd_destroy(cmd);
		if (rc == ACL_DENY) {
			logger(LOG_DEBUG, "access to %s `%s` denied by global ACL", creds, command);
			reply = pdu_reply(pdu, "ERROR", 1, "Permission denied");
			pdu_send_and_free(reply, sock);
			goto REQUEST_exit;
		}

		pdu_t *blast = pdu_make("COMMAND", 3, serial, creds, command);
		pdu_extend(blast, code, len);
		pdu_extendf(blast, "%s", filters);
		pdu_send_and_free(blast, server->broadcast);

		reply = pdu_reply(pdu, "SUBMITTED", 1, serial);
		pdu_send_and_free(reply, sock);

REQUEST_exit:
		/* username is owned by the client object now */
		free(pubkey);
		free(secret);
		free(serial);
		free(creds);
		free(code);
		free(filters);
		return VIGOR_REACTOR_CONTINUE;
	}

	if (strcmp(pdu_type(pdu), "CHECK") == 0) {
		mesh_result_t *r, *r_tmp;
		char *serial = pdu_string(pdu, 1);
		mesh_slot_t *client = cache_get(server->slots, serial);
		free(serial);

		if (client) {
			for_each_object_safe(r, r_tmp, &client->results, l) {
				reply = (r->status && r->output)
					? pdu_reply(pdu, "RESULT", 4, r->fqdn, r->uuid, r->status, r->output)
					: pdu_reply(pdu, "OPTOUT", 2, r->fqdn, r->uuid);

				pdu_send_and_free(reply, sock);
				list_delete(&r->l);
				free(r->fqdn);
				free(r->uuid);
				free(r->status);
				free(r->output);
				free(r);
			}

			reply = pdu_reply(pdu, "DONE", 0);
		} else {
			reply = pdu_reply(pdu, "ERROR", 1, "not a client");
		}
		pdu_send_and_free(reply, sock);
	}

	if (server->_safe_word && strcmp(pdu_type(pdu), server->_safe_word) == 0)
		return VIGOR_REACTOR_HALT;

	return VIGOR_REACTOR_CONTINUE;
}

mesh_client_t* mesh_client_new(void)
{
	mesh_client_t *c = vmalloc(sizeof(mesh_client_t));
	c->acl_default = ACL_DENY;
	return c;
}

int mesh_client_setopt(mesh_client_t *c, int opt, const void *data, size_t len)
{
	assert(c);
	errno = EINVAL;

	switch (opt) {
	case MESH_CLIENT_FQDN:
		free(c->fqdn);
		if (len == 0) {
			c->fqdn = NULL;
		} else {
			c->fqdn = vmalloc(len + 1);
			memcpy(c->fqdn, data, len);
		}
		return 0;

	case MESH_CLIENT_GATHERERS:
		free(c->gatherers);
		if (len == 0) {
			c->gatherers = NULL;
		} else {
			c->gatherers = vmalloc(len + 1);
			memcpy(c->gatherers, data, len);
		}
		return 0;

	case MESH_CLIENT_FACTS:
		if (len != sizeof(hash_t*)) return -1;
		c->facts = (hash_t*)data;
		return 0;

	case MESH_CLIENT_ACL:
		if (len != sizeof(list_t*)) return -1;
		c->acl = (list_t*)data;
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

int mesh_client_handle(mesh_client_t *c, void *sock, pdu_t *pdu)
{
	assert(pdu);
	pdu_t *reply;

	logger(LOG_INFO, "inbound %s packet", pdu_type(pdu));
	if (strcmp(pdu_type(pdu), "COMMAND") == 0) {
		char *serial  = pdu_string(pdu, 1); assert(serial);
		char *creds   = pdu_string(pdu, 2); assert(creds);
		char *command = pdu_string(pdu, 3); assert(command);
		char *filters = pdu_string(pdu, 5); assert(filters);

		size_t n;
		uint8_t *code = pdu_segment(pdu, 4, &n);
		assert(code);
		assert(n > 1);

		cmd_t *cmd = cmd_parse(command, COMMAND_LITERAL);
		assert(cmd);

		logger(LOG_DEBUG, "inbound COMMAND `%s` on behalf of %s", command, creds);
		int disposition = acl_check(c->acl, creds, cmd);
		cmd_destroy(cmd);

		if (disposition == ACL_NEUTRAL)
			disposition = c->acl_default;

		if (disposition == ACL_DENY) {
			logger(LOG_INFO, "denied `%s` to %s per ACL", command, creds);
			goto bail;
		}

		if (!c->facts) {
			c->facts = vmalloc(sizeof(hash_t));
			logger(LOG_DEBUG, "no cached facts; gathering now");
			fact_gather(c->gatherers, c->facts);
		}

		LIST(filter);
		filter_parseall(&filter, filters);
		if (!filter_matchall(&filter, c->facts)) {
			logger(LOG_INFO, "opting out of `%s` command from %s, per filters",
					command, creds);

			reply = pdu_make("OPTOUT", 3,
					serial,
					c->fqdn,
					hash_get(c->facts, "sys.uuid"));

			pdu_send_and_free(reply, sock);
			goto bail;
		}

		vm_t vm;
		int rc = vm_reset(&vm);
		assert(rc == 0);

		rc = vm_load(&vm, code, n);
		assert(rc == 0);

		vm.stdout = tmpfile();
		vm_exec(&vm);

		rewind(vm.stdout);
		char output[8192] = {0};
		if (!fgets(output, 8192, vm.stdout))
			output[0] = '\0';
		fclose(vm.stdout);

		rc = vm_done(&vm);
		assert(rc == 0);

		reply = pdu_make("RESULT", 5,
				serial,
				c->fqdn,
				hash_get(c->facts, "sys.uuid"),
				"0",
				output);

		pdu_send_and_free(reply, sock);

bail:
		free(serial);
		free(creds);
		free(command);
		free(code);
		free(filters);

	} else {
		logger(LOG_DEBUG, "unrecognized PDU: '%s'", pdu_type(pdu));
		/* ignore */
	}

	return 0;
}
