
#include "clockwork.h"
#include "stringlist.h"
#include "hash.h"
#include "policy.h"
#include "spec/parser.h"
#include <getopt.h>

struct command {
	char       *cmd;
	size_t      argc;
	stringlist *args;
	char       *orig;
};
typedef int (*command_fn)(struct command*, int);

#define slv(sl,i) (sl)->strings[(i)]
#define HELP_ROOT "help"

#define LINE_BUFSIZ 1024
#define TOKEN_DELIM " \t"

static struct command* parse_command(const char *s);
static void set_context(int type, const char *name, struct stree *obj);
static void clear_policy(void);
static void make_policy(void);
static void load_facts_from_path(const char *path);
static stringlist* hash_keys(struct hash*);
static int show_help_file(const char *path);
static void show_hash_keys(struct hash *h);
static void show_facts(const char *pattern);
static void show_hosts(void);
static void show_policies(void);
static void show_resources(void);
static void show_resource(const char *type, const char *name);

#define COMMAND(x) static int command_ ## x (struct command *c, int interactive)
COMMAND(about);
COMMAND(help);
COMMAND(quit);
COMMAND(show);
COMMAND(use);
COMMAND(fact);
COMMAND(load);
COMMAND(clear);
COMMAND(log);

struct cwpol_opts {
	char *command;
	char *manifest;
	char *facts;
};

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
static int dispatch(const char *command, int interactive);
static int dispatch1(const char *command, int interactive);

#define PS_TOP "top"

static struct cwpol_opts* cwpol_options(int argc, char **argv);

/** Options

  -e, --execute 'some command'
  -v, --verbose
  -V, --version
  -h, --help
  </path/to/manifest/file>

 */

int main(int argc, char **argv)
{
	const char *prompt = PS_TOP;
	char line[LINE_BUFSIZ];
	struct cwpol_opts *opts;
	int interactive;

	opts = cwpol_options(argc, argv);
	interactive = isatty(0);

	setup();
	if (opts->manifest) {
		DEBUG("pre-loading manifest: %s", opts->manifest);
		dispatch1(opts->manifest, 0);
	}
	if (opts->facts) {
		DEBUG("pre-loading facts: %s", opts->facts);
		dispatch1(opts->facts, 0);
	}
	if (opts->command) {
		exit(dispatch(opts->command, 0));
	}

	if (interactive) {
		printf("%s - Clockwork Policy Shell\n\n", argv[0]);
		printf("Type `about' for information on this program.\n");
		printf("Type `help' to get help on shell commands.\n");
	}

	do {
		if (interactive) {
			switch (CONTEXT.type) {
			case CONTEXT_NONE:   printf("\nglobal> ");                  break;
			case CONTEXT_HOST:   printf("\nhost:%s> ",   CONTEXT.name); break;
			case CONTEXT_POLICY: printf("\npolicy:%s> ", CONTEXT.name); break;
			default: printf("\n> "); break;
			}
		}

		if (!fgets(line, LINE_BUFSIZ, stdin)) { break; }
	} while (!dispatch(line, interactive));

	return 0;
}

static struct command* parse_command(const char *s)
{
	struct command *c;
	char *tok, *ctx;
	size_t n;

	c = xmalloc(sizeof(struct command));
	c->orig = strdup(s);

	/* kill the newline */
	n = strlen(c->orig);
	if (c->orig[n-1] == '\n') { c->orig[n-1] = '\0'; }

	c->cmd = strtok_r(c->orig, TOKEN_DELIM, &ctx);
	if (!c->cmd) {
		free(c);
		return NULL;
	}

	c->argc = 0;
	c->args = stringlist_new(NULL);
	while (tok = strtok_r(NULL, TOKEN_DELIM, &ctx)) {
		stringlist_add(c->args, tok);
		c->argc++;
	}

	return c;
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
	int main;
	char *path;

	if (c->argc >= 1) {
		main = 0;
		path = string(HELP_ROOT "/%s.help", slv(c->args, 0));
	} else {
		main = 1;
		path = strdup(HELP_ROOT "/main");
	}

	if (show_help_file(path) != 0 && interactive) {
		if (main) {
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
	char *type, *target;
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
		ERROR("No manifest loaded");
		return 0;
	}

	if (strcmp(type, "host") == 0) {
		if (!(root = hash_get(MANIFEST->hosts, target))) {
			ERROR("No such host '%s'", target);
		} else {
			set_context(CONTEXT_HOST, target, root);
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

		hash_set(FACTS, k, v);
		xfree(k);

		show_facts(NULL);
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

static stringlist *hash_keys(struct hash *h)
{
	struct hash_cursor cur;
	char *k, *v;
	stringlist *l;

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
	stringlist *keys = hash_keys(h);

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

	stringlist *keys = hash_keys(FACTS);
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
	stringlist *list;
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
	stringlist *keys;
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

static struct cwpol_opts* cwpol_options(int argc, char **argv)
{
	struct cwpol_opts *o;
	const char *short_opts = "h?e:f:vV";
	struct option long_opts[] = {
		{ "help",    no_argument,       NULL, 'h' },
		{ "execute", required_argument, NULL, 'e' },
		{ "facts",   required_argument, NULL, 'f' },
		{ "verbose", no_argument,       NULL, 'v' },
		{ "version", no_argument,       NULL, 'V' },
		{ 0, 0, 0, 0 },
	};

	int v = LOG_LEVEL_ERROR;
	int opt, idx = 0;

	o = xmalloc(sizeof(struct cwpol_opts));

	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch(opt) {
		case 'h':
		case '?':
			free(o->command);
			o->command = strdup("help");
			break;

		case 'e':
			free(o->command);
			o->command = strdup(optarg);
			break;

		case 'f':
			free(o->facts);
			o->facts = string("load facts from %s", optarg);
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

static int dispatch(const char *c, int interactive)
{
	stringlist *commands;
	size_t i;
	int rc = 0, t;

	commands = stringlist_split(c, strlen(c), ";");
	for_each_string(commands, i) {
		t = dispatch1(slv(commands, i), interactive);
		if (t && !interactive) {
			return rc;
		}

		if (!rc && t > 0) { rc = t; }
	}
	return rc;
}

static int dispatch1(const char *c, int interactive)
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
		rc = (*f)(command, interactive);
	} else {
		ERROR("Unknown command: %s", command->cmd);
		rc = -1;
	}
	free(command); /* FIXME: good enough? */
	return rc;
}


