/*
  Copyright 2011 James Hunt <james@jameshunt.us>

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
#include "policy.h"
#include "spec/parser.h"
#include <getopt.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

struct cwpol_opts {
	char *command;
	char *manifest;
	char *facts;
	char *cache;

	int no_clobber;
};

struct command {
	char       *cmd;
	size_t      argc;
	struct stringlist *args;
	char       *orig;
};
typedef int (*command_fn)(struct cwpol_opts *o, struct command*, int);

#define slv(sl,i) (sl)->strings[(i)]
#ifdef DEVEL
#  define HELP_ROOT "share/help"
#else
#  define HELP_ROOT CW_DATADIR "/help"
#endif

#define TOKEN_DELIM " \t"

static struct command* parse_command(const char *s);
static void free_command(struct command*);
static void set_context(int type, const char *name, struct stree *obj);
static void clear_policy(void);
static void make_policy(void);
static void load_facts_from_path(const char *path);
static struct stringlist* hash_keys(struct hash*);
static int show_help_file(const char *path);
static void show_hash_keys(struct hash *h);
static void show_facts(const char *pattern);
static void show_fact(const char *name);
static void show_hosts(void);
static void show_policies(void);
static void show_resources(void);
static void show_resource(const char *type, const char *name);

#define COMMAND(x) static int command_ ## x (struct cwpol_opts *o, struct command *c, int interactive)
COMMAND(about);
COMMAND(help);
COMMAND(quit);
COMMAND(show);
COMMAND(use);
COMMAND(fact);
COMMAND(load);
COMMAND(clear);
COMMAND(log);

#define CONTEXT_NONE   0
#define CONTEXT_HOST   1
#define CONTEXT_POLICY 2
static struct {
	int type;
	char *name;
	struct stree *root;
	struct policy *policy;
} CONTEXT = {
	.type = CONTEXT_NONE,
	.name = NULL,
	.root = NULL,
	.policy = NULL
};
static struct hash     *DISPATCH = NULL;
static struct hash     *FACTS    = NULL;
static struct manifest *MANIFEST = NULL;

static void setup(void);
static int dispatch(struct cwpol_opts *o, const char *command, int interactive);
static int dispatch1(struct cwpol_opts *o, const char *command, int interactive);
static struct cwpol_opts* cwpol_options(int argc, char **argv, int interactive);

/* Options

  -e, --execute 'some command'
  -v, --verbose
  -V, --version
  -h, --help
  </path/to/manifest/file>

 */

int main(int argc, char **argv)
{
	struct cwpol_opts *opts;
	int interactive;
	char *hist = NULL;

	interactive = isatty(0);
	opts = cwpol_options(argc, argv, interactive);

	setup();
	if (opts->manifest) {
		DEBUG("pre-loading manifest: %s", opts->manifest);
		dispatch1(opts, opts->manifest, 0);
	}
	if (opts->facts) {
		DEBUG("pre-loading facts: %s", opts->facts);
		dispatch1(opts, opts->facts, 0);
	}
	if (opts->command) {
		exit(dispatch(opts, opts->command, 0));
	}

	if (interactive) {
		printf("%s - Clockwork Policy Shell\n\n", argv[0]);
		printf("Type `about' for information on this program.\n");
		printf("Type `help' to get help on shell commands.\n");

		hist = string("%s/.cwpol_history", getenv("HOME"));
		if (hist) {
			using_history();
			read_history(hist);
		}
	}

	int have_output = 1;
	char *ps1 = NULL;
	char *rline = NULL, *line = NULL;
	do {
		if (interactive) {
			if (have_output) { printf("\n"); }
			free(ps1);
			switch (CONTEXT.type) {
			case CONTEXT_NONE:   ps1 = string("global> ");                  break;
			case CONTEXT_HOST:   ps1 = string("host:%s> ", CONTEXT.name);   break;
			case CONTEXT_POLICY: ps1 = string("policy:%s> ", CONTEXT.name); break;
			default: ps1 = string("> "); break;
			}
		}

		have_output = 0;
		xfree(rline);
		rline = readline(ps1);
		if (!rline) { break; }

		for (line = rline; *line && isspace(*line); line++)
			;
		if (!*line) { continue; }
		have_output = 1;
		add_history(line);
	} while (!dispatch(opts, line, interactive));

	printf("\n"); /* prettier on ^D */

	if (hist) {
		write_history(hist);
	}

	return 0;
}

static struct command* parse_command(const char *s)
{
	struct command *c;
	char *tmp, *tok, *ctx;

	c = xmalloc(sizeof(struct command));

	tmp = strdup(s);
	c->cmd = strtok_r(tmp, TOKEN_DELIM, &ctx);
	if (!c->cmd) {
		free(c);
		free(tmp);
		return NULL;
	}

	c->orig = strdup(s);
	c->argc = 0;
	c->args = stringlist_new(NULL);
	while ((tok = strtok_r(NULL, TOKEN_DELIM, &ctx)) != NULL) {
		stringlist_add(c->args, tok);
		c->argc++;
	}

	return c;
}

static void free_command(struct command* c)
{
	if (c) {
		free(c->orig);
		stringlist_free(c->args);
	}
	free(c);
}

COMMAND(about)
{
	if (show_help_file(HELP_ROOT "/about.help")) {
		ERROR("Can't find help files.");
		WARNING("You may want to check your installation.");
	}
	return 0;
}

COMMAND(help)
{
	int use_main;
	char *path;

	if (c->argc >= 1) {
		use_main = 0;
		path = string(HELP_ROOT "/%s.help", slv(c->args, 0));
	} else {
		use_main = 1;
		path = strdup(HELP_ROOT "/main");
	}

	if (show_help_file(path) != 0 && interactive) {
		if (use_main) {
			ERROR("Can't find main help file.");
			INFO("You may want to check your installation.");
		} else {
			ERROR("Nothing known about '%s'.", slv(c->args,0));
			INFO("Try 'help' for a list of commands.");
		}
	}
	return 0;
}

COMMAND(quit)
{
	INFO("Goodbye...");
	return 1;
}

COMMAND(show)
{
	char *type = c->argc > 0 ? slv(c->args,0) : NULL;

	if (c->argc == 1 && strcmp(type, "facts") == 0) {
		show_facts(NULL);

	} else if (c->argc == 3 && strcmp(type, "facts") == 0
	           && strcmp(slv(c->args,1), "like") == 0) {
		show_facts(slv(c->args,2));

	} else if (c->argc == 2 && strcmp(type, "fact") == 0) {
		show_fact(slv(c->args,1));

	} else if (c->argc == 1 && strcmp(type, "hosts") == 0) {
		show_hosts();

	} else if (c->argc == 1 && strcmp(type, "policies") == 0) {
		show_policies();

	} else if (c->argc == 1 && strcmp(type, "resources") == 0) {
		if (!CONTEXT.root) {
			ERROR("Invalid referential context");
			INFO("Select a host or policy through the 'use' command");
			return 0;
		}

		show_resources();

	} else if (c->argc == 2) {
		if (!CONTEXT.root) {
			ERROR("Invalid referential context");
			INFO("Select a host or policy through the 'use' command");
			return 0;
		}

		show_resource(slv(c->args,0), slv(c->args,1));
	} else {
		ERROR("Missing required arguments");
		INFO("   show fact <name>");
		INFO("   show facts [like pattern]");
		INFO("   show hosts");
		INFO("   show policies");
		INFO("   show resources");
		INFO("   show <resource-type> <name>");
	}
	return 0;
}

COMMAND(use)
{
	char *type = NULL, *target;
	struct stree *root;

	if (c->argc >= 1) {
		type = slv(c->args,0);
	}

	if (c->argc == 2) {
		target = slv(c->args,1);
	} else if (type && strcmp(type, "global") == 0) {
		set_context(CONTEXT_NONE, NULL, NULL);
		return 0;
	} else {
		ERROR("Missing required arguments");
		INFO("   use global");
		INFO("   use host <hostname>");
		INFO("   use policy <policy>");
		return 0;
	}

	if (!MANIFEST) {
		ERROR("No manifest loaded (try `load <filename>'");
		return 0;
	}

	if (strcmp(type, "host") == 0) {
		if (!(root = hash_get(MANIFEST->hosts, target))) {
			WARNING("Host '%s' not explicitly defined; falling back to default", target);
			if (!(root = MANIFEST->fallback)) {
				ERROR("No default host defined", target);
			}
		}

		if (root) {
			set_context(CONTEXT_HOST, target, root);

			/* try to load facts */
			if (o->no_clobber == 0) {
				char *path = string("%s/%s.facts", o->cache, target);
				INFO("auto-loading host facts from %s", o->cache);
				load_facts_from_path(path);
				free(path);

			} else if (interactive) {
				INFO("using previously loaded / defined facts");
				INFO("you may want to `clear facts' and");
				INFO("`use host %s' again", target);
			}
		}
	} else if (strcmp(type, "policy") == 0) {
		if (!(root = hash_get(MANIFEST->policies, target))) {
			ERROR("No such policy '%s'", target);
		} else {
			set_context(CONTEXT_POLICY, target, root);
		}
	} else {
		ERROR("Unknown context type: '%s'", type);
	}
	return 0;
}

COMMAND(fact)
{
	char *k, *v;
	char *a, *b;
	if (c->args > 0) {

		a = c->orig;
		/* skip leading whitespace */
		for (; *a && isspace(*a); a++);
		/* skip "fact" command name */
		for (; *a && !isspace(*a); a++);
		/* skip whitespace */
		for (; *a && isspace(*a); a++);
		/* get fact */
		for (b = a; *b && !isspace(*b) && *b != '='; b++);
		k = xmalloc(b-a + 1);
		memcpy(k, a, b-a);

		/* skip whitespace + '=' */
		for (a = b; *a && (isspace(*a) || *a == '='); a++);
		v = strdup(a);

		if (!*k || !*v) {
			ERROR("Malformed fact.  See `help fact'");
			return 0;
		}

		hash_set(FACTS, k, v);
		xfree(k);

		o->no_clobber = 1;
		if (interactive) {
			show_facts(NULL);
		}
		return 0;
	}

	ERROR("Missing required arguments");
	INFO("   fact new.fact.name = fact.value");
	return 0;
}

COMMAND(load)
{
	if (c->argc == 3
	 && strcmp(slv(c->args,0), "facts") == 0
	 && strcmp(slv(c->args,1), "from")  == 0) {
		o->no_clobber = 1;
		load_facts_from_path(slv(c->args,2));
		if (interactive) { show_facts(NULL); }
		return 0;
	}

	if (c->argc == 1) {
		if (interactive) { INFO("Reading in %s", slv(c->args,0)); }

		manifest_free(MANIFEST);
		MANIFEST = parse_file(slv(c->args,0));
		if (!MANIFEST) {
			INFO("Failed to load manifest");
			return 0;
		}

		if (interactive) { INFO("Loaded manifest."); }
		return 0;
	}

	ERROR("Missing required arguments");
	INFO("   load facts from /path/to/file");
	INFO("   load /path/to/manifest.pol");

	return 0;
}

COMMAND(clear)
{
	if (c->argc == 1
	 && strcmp(slv(c->args,0), "facts") == 0) {
		hash_free_all(FACTS);
		FACTS = hash_new();
		o->no_clobber = 0;
		return 0;
	}

	ERROR("Missing required arguments");
	INFO("   clear facts");
	return 0;
}

COMMAND(log)
{
	static const char *levels[] = {
		NULL,
		"none",
		"critical",
		"error",
		"warning",
		"notice",
		"info",
		"debug",
		"all"
	};
	int i;
	char *arg;

	if (c->argc != 1) {
		i = log_level();
		printf("log level is %s (%i)\n", levels[i], i);
		return 0;
	}
	arg = slv(c->args, 0);
	for (i = 1; i <= LOG_LEVEL_ALL; i++) {
		if (strcasecmp(levels[i], arg) == 0) {
			log_set(i);
			if (interactive) {
				printf("log level set to %s (%i)\n", arg, i);
			}
			return 0;
		}
	}
	ERROR("Unknown log level: '%s'", arg);
	return 0;
}

static void set_context(int type, const char *name, struct stree *obj)
{
	xfree(CONTEXT.name);
	CONTEXT.name = xstrdup(name);
	CONTEXT.type = type;
	CONTEXT.root = obj;
	clear_policy();
	make_policy();
}

static void clear_policy(void)
{
	policy_free(CONTEXT.policy);
	CONTEXT.policy = NULL;
}
static void make_policy(void)
{
	if (CONTEXT.policy || !CONTEXT.root) { return; }
	CONTEXT.policy = policy_generate(CONTEXT.root, FACTS);
}

static void load_facts_from_path(const char *path)
{
	FILE* io = fopen(path, "r");

	if (!io) {
		ERROR("Load failed:%s: %s", path, strerror(errno));
		return;
	}

	if (!fact_read(io, FACTS)) {
		ERROR("Load failed");
	}
	fclose(io);
}

static struct stringlist *hash_keys(struct hash *h)
{
	struct hash_cursor cur;
	char *k, *v;
	struct stringlist *l;

	l = stringlist_new(NULL);
	for_each_key_value(h, &cur, k, v) {
		stringlist_add(l, k);
	}

	return l;
}

static int show_help_file(const char *path)
{
	FILE *io;
	char buf[8192];

	io = fopen(path, "r");
	if (!io) { return -1; }

	while (fgets(buf, 8192, io)) {
		printf("%s", buf);
	}

	fclose(io);
	return 0;
}

static void show_hash_keys(struct hash *h)
{
	size_t i;
	struct stringlist *keys = hash_keys(h);

	stringlist_sort(keys, STRINGLIST_SORT_ASC);
	if (keys->num == 0) {
		INFO("(none defined)");
	} else {
		for_each_string(keys, i) {
			printf("%s\n", slv(keys,i));
		}
	}
}

static void show_facts(const char *pattern)
{
	char *k, *v;
	size_t i;
	size_t cmpn = (pattern ? strlen(pattern) : 0);

	struct stringlist *keys = hash_keys(FACTS);
	if (keys->num == 0) {
		INFO("(none defined)");
		return;
	}

	stringlist_sort(keys, STRINGLIST_SORT_ASC);
	for_each_string(keys,i) {
		k = slv(keys, i);
		v = hash_get(FACTS, k);

		if (!pattern || strncmp(pattern, k, cmpn) == 0) {
			printf("%s = %s\n", k, v);
		}
	}
}

static void show_fact(const char *name)
{
	char *v;

	v = hash_get(FACTS, name);
	if (v) {
		printf("%s = %s\n", name, v);
	} else {
		printf("fact %s not defined\n", name);
	}
}

static void show_hosts(void)
{
	if (!MANIFEST) {
		ERROR("No manifest loaded");
		return;
	}
	show_hash_keys(MANIFEST->hosts);
}

static void show_policies(void)
{
	if (!MANIFEST) {
		ERROR("No manifest loaded");
		return;
	}
	show_hash_keys(MANIFEST->policies);
}

static void show_resources(void)
{
	struct stringlist *list;
	struct resource *r;
	size_t i;
	char type[256], *key;

	make_policy();
	if (!CONTEXT.policy) {
		ERROR("Failed to generate policy");
		return;
	}

	list = stringlist_new(NULL);
	for_each_resource(r, CONTEXT.policy) {
		stringlist_add(list, r->key);
	}
	if (list->num == 0) {
		INFO("(none defined)");
		return;
	}

	stringlist_sort(list, STRINGLIST_SORT_ASC);
	for_each_string(list, i) {
		strncpy(type, slv(list,i), 255); type[255] = '\0';

		for (key = type; *key && *key != ':'; key++)
			;
		*key++ = '\0';

		printf("%9s %s\n", type, key);
	}
}

static void show_resource(const char *type, const char *name)
{
	struct resource *r;
	char *target = string("%s:%s", type, name);
	struct hash *attrs;
	struct stringlist *keys;
	size_t i;
	char *value;
	size_t maxlen = 0, n;
	char *fmt;

	make_policy();
	if (!CONTEXT.policy) {
		ERROR("Failed to generate policy");
		return;
	}

	if (!(r = hash_get(CONTEXT.policy->index, target))) {
		ERROR("!! No such %s resource: %s", type, name);
	} else {
		attrs = resource_attrs(r);
		keys = hash_keys(attrs);
		stringlist_sort(keys, STRINGLIST_SORT_ASC);

		maxlen = 0;
		for_each_string(keys, i) {
			n = strlen(slv(keys,i));
			maxlen = (n > maxlen ? n : maxlen);
		}

		fmt = string("  %%-%us: \"%%s\"\n", maxlen+1);

		printf("\n");
		printf("%s \"%s\" {\n", type, name);
		for_each_string(keys, i) {
			if ((value = hash_get(attrs, slv(keys,i))) != NULL) {
				printf(fmt, slv(keys,i), value);
			} else {
				printf(" # %s not specified\n", slv(keys,i));
			}
		}
		printf("}\n");

		hash_free_all(attrs);
	}

	free(target);
}

static struct cwpol_opts* cwpol_options(int argc, char **argv, int interactive)
{
	struct cwpol_opts *o;
	const char *short_opts = "h?e:f:vV";
	struct option long_opts[] = {
		{ "help",    no_argument,       NULL, 'h' },
		{ "execute", required_argument, NULL, 'e' },
		{ "facts",   required_argument, NULL, 'f' },
		{ "cache",   required_argument, NULL, 'c' },
		{ "verbose", no_argument,       NULL, 'v' },
		{ "version", no_argument,       NULL, 'V' },
		{ 0, 0, 0, 0 },
	};

	int v = (interactive ? LOG_LEVEL_INFO : LOG_LEVEL_ERROR);
	int opt, idx = 0;

	o = xmalloc(sizeof(struct cwpol_opts));
	o->cache = strdup(CW_CACHEDIR "/facts");
	o->no_clobber = 0;

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch(opt) {
		case 'h':
		case '?':
			free(o->command);
			o->command = strdup("help cli");
			break;

		case 'e':
			free(o->command);
			o->command = strdup(optarg);
			break;

		case 'f':
			free(o->facts);
			o->facts = string("load facts from %s", optarg);
			break;

		case 'c':
			free(o->cache);
			o->cache = strdup(optarg);
			break;

		case 'v':
			v++;
			break;

		case 'V':
			free(o->command);
			o->command = strdup("about");
			break;
		}
	}

	if (optind == argc - 1) {
		free(o->manifest);
		o->manifest = string("load %s", argv[optind]);
	} else if (optind < argc) {
		free(o->command);
		o->command = strdup("help");
	}

	log_set(v);
	return o;
}

static void setup(void)
{
	DISPATCH = hash_new();
	hash_set(DISPATCH, "about",   command_about);

	hash_set(DISPATCH, "help",    command_help);
	hash_set(DISPATCH, "?",       command_help);

	hash_set(DISPATCH, "q",       command_quit);
	hash_set(DISPATCH, "quit",    command_quit);
	hash_set(DISPATCH, "exit",    command_quit);

	hash_set(DISPATCH, "show",    command_show);
	hash_set(DISPATCH, "use",     command_use);
	hash_set(DISPATCH, "fact",    command_fact);
	hash_set(DISPATCH, "load",    command_load);
	hash_set(DISPATCH, "clear",   command_clear);

	hash_set(DISPATCH, "log",     command_log);

	FACTS = hash_new();

	MANIFEST = NULL;
}

static int dispatch(struct cwpol_opts *o, const char *c, int interactive)
{
	struct stringlist *commands;
	size_t i;
	int rc = 0, t;

	commands = stringlist_split(c, strlen(c), ";", SPLIT_GREEDY);
	for_each_string(commands, i) {
		t = dispatch1(o, slv(commands, i), interactive);
		if (t && !interactive) {
			rc = t;
			break;
		}

		if (!rc && t > 0) { rc = t; }
	}
	stringlist_free(commands);
	return rc;
}

static int dispatch1(struct cwpol_opts *o, const char *c, int interactive)
{
	command_fn f;
	struct command *command;
	int rc;

	command = parse_command(c);
	if (!command) {
		if (!interactive) {
			ERROR("Failed to execute '%s'", c);
		}
		return 1;
	}

	DEBUG("dispatching '%s'", command->cmd);
	if ((f = hash_get(DISPATCH, command->cmd))) {
		rc = (*f)(o, command, interactive);
	} else {
		ERROR("Unknown command: %s", command->cmd);
		rc = -1;
	}
	free_command(command);
	return rc;
}


