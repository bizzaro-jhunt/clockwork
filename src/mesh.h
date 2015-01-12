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

#ifndef __MESH_H
#define __MESH_H

#include <vigor.h>
#include "base.h"

/*
     ######  ##     ## ########
    ##    ## ###   ### ##     ##
    ##       #### #### ##     ##
    ##       ## ### ## ##     ##
    ##       ##     ## ##     ##
    ##    ## ##     ## ##     ##
     ######  ##     ## ########

 */

#define CMD_UNKNOWN 0
#define CMD_PING    1
#define CMD_SHOW    2
#define CMD_QUERY   3

#define COMMAND_LITERAL 0
#define COMMAND_PATTERN 1

#define COMMAND_TOKEN_LITERAL  0
#define COMMAND_TOKEN_WILDCARD 1

typedef struct {
	list_t l;
	int   type;
	char *value;
} cmd_token_t;

typedef struct {
	int        pattern;
	int        type;
	char      *string;
	list_t     tokens;
} cmd_t;

cmd_t* cmd_new(void);
void cmd_destroy(cmd_t*);
cmd_t* cmd_parse(const char*, int);
cmd_t* cmd_parsev(const char**, int);
int cmd_match(cmd_t*, cmd_t*);
int cmd_gencode(cmd_t*, FILE*);

/*

       ###     ######  ##
      ## ##   ##    ## ##
     ##   ##  ##       ##
    ##     ## ##       ##
    ######### ##       ##
    ##     ## ##    ## ##
    ##     ##  ######  ########

 */

#define ACL_NEUTRAL 0
#define ACL_ALLOW   1
#define ACL_DENY    2

typedef struct {
	list_t l;

	char  *target_user;
	char  *target_group;
	cmd_t *pattern;

	int   disposition;
	int   is_final;
} acl_t;

acl_t* acl_new(void);
void acl_destroy(acl_t*);
acl_t* acl_parse(const char*);
char *acl_string(acl_t*);
int acl_read(list_t*, const char*);
int acl_readio(list_t*, FILE*);
int acl_write(list_t*, const char*);
int acl_writeio(list_t*, FILE*);
int acl_gencode(acl_t*, FILE*);
int acl_match(acl_t*, const char*, cmd_t*);
int acl_check(list_t*, const char*, cmd_t*);

/*

    ######## #### ##       ######## ######## ########
    ##        ##  ##          ##    ##       ##     ##
    ##        ##  ##          ##    ##       ##     ##
    ######    ##  ##          ##    ######   ########
    ##        ##  ##          ##    ##       ##   ##
    ##        ##  ##          ##    ##       ##    ##
    ##       #### ########    ##    ######## ##     ##

 */

typedef struct {
	char *fact;
	int   match;
	pcre *regex;
	char *literal;

	list_t l;
} filter_t;

filter_t* filter_new(void);
void filter_destroy(filter_t*);
filter_t* filter_parse(const char*);
int filter_parseall(list_t*, const char*);
int filter_match(filter_t*, hash_t*);
int filter_matchall(list_t*, hash_t*);

/*

    ##     ## ########  ######  ##     ##
    ###   ### ##       ##    ## ##     ##
    #### #### ##       ##       ##     ##
    ## ### ## ######    ######  #########
    ##     ## ##             ## ##     ##
    ##     ## ##       ##    ## ##     ##
    ##     ## ########  ######  ##     ##

 */

typedef struct {
	void       *zmq;
	int         zmq_auto;

	void       *control;
	void       *broadcast;

	uint64_t    serial;
	char       *pam_service;
	trustdb_t  *trustdb;

	list_t      acl;
	cache_t    *slots;

	cert_t     *cert;
	void       *zap;

	char       *_safe_word;
} mesh_server_t;

typedef struct {
	char      *fqdn;
	char      *gatherers;

	hash_t    *facts;
	list_t    *acl;
	int        acl_default;
} mesh_client_t;

#define MESH_SERVER_SERIAL       1
#define MESH_SERVER_PAM_SERVICE  2
#define MESH_SERVER_CERTIFICATE  3
#define MESH_SERVER_SAFE_WORD    4
#define MESH_SERVER_CACHE_LIFE   5
#define MESH_SERVER_CACHE_SIZE   6
#define MESH_SERVER_GLOBAL_ACL   7
#define MESH_SERVER_TRUSTDB      8

mesh_server_t* mesh_server_new(void *zmq);
int mesh_server_setopt(mesh_server_t *s, int opt, const void *data, size_t len);
int mesh_server_bind_control(mesh_server_t *s, const char *endpoint);
int mesh_server_bind_broadcast(mesh_server_t *s, const char *endpoint);
int mesh_server_run(mesh_server_t *s);
void mesh_server_destroy(mesh_server_t *s);
int mesh_server_reactor(void *sock, pdu_t *pdu, void *data);

#define MESH_CLIENT_FQDN         1
#define MESH_CLIENT_GATHERERS    2
#define MESH_CLIENT_FACTS        3
#define MESH_CLIENT_ACL          4
#define MESH_CLIENT_ACL_DEFAULT  5

mesh_client_t* mesh_client_new(void);
int mesh_client_setopt(mesh_client_t *c, int opt, const void *data, size_t len);
void mesh_client_destroy(mesh_client_t *c);
int mesh_client_handle(mesh_client_t *c, void *sock, pdu_t *pdu);

#endif
