#include "clockwork.h"
#include "stringlist.h"
#include "hash.h"
#include "policy.h"
#include "spec/parser.h"

struct command {
	char       *cmd;
	size_t      argc;
	stringlist *args;
	char       *orig;
};
typedef int (*command_fn)(struct command*);

#define slv(sl,i) (sl)->strings[(i)]
#define HELP_ROOT "help"

#define LINE_BUFSIZ 1024
#define TOKEN_DELIM " \t"

static struct command* parse_command(char *s);
static void set_context(int type, const char *name, struct stree *obj);
static void clear_policy(void);
static void make_policy(void);
static void load_facts_from_path(const char *path);
/* FIXME: move hash_keys to hash.o ?? */
static stringlist* hash_keys(struct hash*);
/* FIXME: move for_each_string to stringlist.h ?? */
#define for_each_string(sl,i) for ((i)=0; (i)<(sl)->num; (i)++)
static int show_help_file(const char *path);
static void show_hash_keys(struct hash *h);
static void show_facts(void);
static void show_hosts(void);
static void show_policies(void);
static void show_resources(void);
static void show_resource(const char *type, const char *name);

#define COMMAND(x) static int command_ ## x (struct command *c)
COMMAND(about);
COMMAND(help);
COMMAND(quit);
COMMAND(show);
COMMAND(use);
COMMAND(fact);
COMMAND(load);
COMMAND(clear);

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
static struct hash     *FACTS = NULL;
static struct manifest *MANIFEST = NULL;

#define PS_TOP "top"

int main(int argc, char **argv)
{
	int done = 0;
	const char *prompt = PS_TOP;
	char line[LINE_BUFSIZ];
	struct command *command;
	struct hash *dispatch;
	command_fn run;

	if (isatty(0)) {
		printf("%s - Clockwork Policy Shell\n\n", argv[0]);
		printf("Type `about' for information on this program.\n");
		printf("Type `help' to get help on shell commands.\n");
	}

	dispatch = hash_new();
	hash_set(dispatch, "about",   command_about);

	hash_set(dispatch, "help",    command_help);
	hash_set(dispatch, "?",       command_help);

	hash_set(dispatch, "q",       command_quit);
	hash_set(dispatch, "quit",    command_quit);
	hash_set(dispatch, "exit",    command_quit);

	hash_set(dispatch, "show",    command_show);
	hash_set(dispatch, "use",     command_use);
	hash_set(dispatch, "fact",    command_fact);
	hash_set(dispatch, "load",    command_load);
	hash_set(dispatch, "clear",   command_clear);

	FACTS = hash_new();
	MANIFEST = NULL;

	do {
		if (isatty(0)) { /* only show the prompt if we are interactive */
			switch (CONTEXT.type) {
			case CONTEXT_NONE:   printf("\nglobal> ");                  break;
			case CONTEXT_HOST:   printf("\nhost:%s> ",   CONTEXT.name); break;
			case CONTEXT_POLICY: printf("\npolicy:%s> ", CONTEXT.name); break;
			default: printf("\n> "); break;
			}
		}

		if (!fgets(line, LINE_BUFSIZ, stdin)) { break; }
		if (!(command = parse_command(line))) { continue; }

		run = hash_get(dispatch, command->cmd);
		if (run) {
			done = (*run)(command);
		} else {
			printf("!! Unknown command: %s\n", command->cmd);
		}
	} while (!done);

	return 0;
}

static struct command* parse_command(char *s)
{
	struct command *c;
	char *tok, *ctx;
	size_t n;

	/* kill the newline */
	n = strlen(s);
	if (s[n-1] == '\n') { s[n-1] = '\0'; }

	c = xmalloc(sizeof(struct command));
	c->orig = strdup(s);
	c->cmd = strtok_r(s, TOKEN_DELIM, &ctx);
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
		printf("!! Can't find help files...\n");
		printf("You may want to check your installation.\n");
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

	if (!show_help_file(path)) {
		if (main) {
			printf("!! Can't find main help file...\n");
			printf("You may want to check your installation.\n");
		} else {
			printf("!! Nothing known about '%s'.\n", slv(c->args,0));
			printf("Try 'help' for a list of commands.\n");
		}
	}
	return 0;
}

COMMAND(quit)
{
	printf("Goodbye...\n");
	return 1;
}

COMMAND(show)
{
	char *type = c->argc > 0 ? slv(c->args,0) : NULL;

	if (c->argc == 1 && strcmp(type, "facts") == 0) {
		show_facts();

	} else if (c->argc == 1 && strcmp(type, "hosts") == 0) {
		show_hosts();

	} else if (c->argc == 1 && strcmp(type, "policies") == 0) {
		show_policies();

	} else if (c->argc == 1 && strcmp(type, "resources") == 0) {
		if (!CONTEXT.root) {
			printf("!! Invalid referential context\n"
			       "!! Select a host or policy through the 'use' command\n");
			return 0;
		}

		show_resources();

	} else if (c->argc == 2) {
		if (!CONTEXT.root) {
			printf("!! Invalid referential context\n"
			       "!! Select a host or policy through the 'use' command\n");
			return 0;
		}

		show_resource(slv(c->args,0), slv(c->args,1));
	} else {
		printf("!! Missing required arguments\n");
		printf("      show facts\n");
		printf("      show hosts\n");
		printf("      show policies\n");
		printf("      show resources\n");
		printf("      show <resource-type> <name>\n");
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
		printf("!! Missing required arguments\n");
		printf("      use global\n");
		printf("      use host <hostname>\n");
		printf("      use policy <policy>\n");
		return 0;
	}

	if (!MANIFEST) {
		printf("!! No manifest loaded\n");
		return 0;
	}

	if (strcmp(type, "host") == 0) {
		if (!(root = hash_get(MANIFEST->hosts, target))) {
			printf("!! No such host '%s'\n", target);
		} else {
			set_context(CONTEXT_HOST, target, root);
		}
	} else if (strcmp(type, "policy") == 0) {
		if (!(root = hash_get(MANIFEST->policies, target))) {
			printf("!! No such policy '%s'\n", target);
		} else {
			set_context(CONTEXT_POLICY, target, root);
		}
	} else {
		printf("!! Unknown context type: '%s'\n", type);
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

		show_facts();
		return 0;
	}

	printf("!! Missing required arguments\n");
	printf("      fact new.fact.name = fact.value\n");
	return 0;
}

COMMAND(load)
{
	if (c->argc == 3
	 && strcmp(slv(c->args,0), "facts") == 0
	 && strcmp(slv(c->args,1), "from")  == 0) {
		load_facts_from_path(slv(c->args,2));
		show_facts();
		return 0;
	}

	if (c->argc == 1) {
		printf("Reading in %s\n", slv(c->args,0));

		manifest_free(MANIFEST);
		MANIFEST = parse_file(slv(c->args,0));
		if (!MANIFEST) {
			printf("!! Failed to load manifest\n");
			return 0;
		}

		printf("Loaded manifest.\n");
		return 0;
	}

	printf("!! Missing required arguments\n");
	printf("      load facts from /path/to/file\n");
	printf("      load /path/to/manifest.pol\n");

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

	printf("!! Missing required arguments\n");
	printf("      clear facts\n");
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
		printf("!! Load failed:%s: %s\n", path, strerror(errno));
		return;
	}

	if (!fact_read(io, FACTS)) {
		printf("!! Load failed\n");
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
		printf("(none defined)\n");
	} else {
		for_each_string(keys, i) {
			printf("%s\n", slv(keys,i));
		}
	}
}

static void show_facts(void)
{
	char *k, *v;
	size_t i;

	stringlist *keys = hash_keys(FACTS);
	if (keys->num == 0) {
		printf("(none defined)\n");
		return;
	}

	stringlist_sort(keys, STRINGLIST_SORT_ASC);
	for_each_string(keys,i) {
		k = slv(keys, i);
		v = hash_get(FACTS, k);
		printf("%s = %s\n", k, v);
	}
}

static void show_hosts(void)
{
	if (!MANIFEST) {
		printf("!! No manifest loaded\n");
		return;
	}
	show_hash_keys(MANIFEST->hosts);
}

static void show_policies(void)
{
	if (!MANIFEST) {
		printf("!! No manifest loaded\n");
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
		printf("!! Failed to generate policy\n");
		return;
	}

	list = stringlist_new(NULL);
	for_each_resource(r, CONTEXT.policy) {
		stringlist_add(list, r->key);
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
		printf("!! Failed to generate policy\n");
		return;
	}

	if (!(r = hash_get(CONTEXT.policy->index, target))) {
		printf("!! No such %s resource: %s\n", type, name);
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
