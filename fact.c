#include <assert.h>
#include <sys/utsname.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "fact.h"
#include "mem.h"

struct fact* fact_new(const char *name, const char *value)
{
	struct fact *fact;

	fact = malloc(sizeof(struct fact));
	if (!fact) {
		return NULL;
	}

	if (fact_init(fact, name, value) != 0) {
		free(fact);
		return NULL;
	}

	return fact;
}

int fact_init(struct fact* fact, const char *name, const char *value)
{
	assert(fact);
	assert(name);
	assert(value);

	fact->name  = xstrdup(name);
	fact->value = xstrdup(value);

	return 0;
}

void fact_deinit(struct fact *fact)
{
	assert(fact);

	xfree(fact->name);
	xfree(fact->value);
}

void fact_free(struct fact *fact)
{
	assert(fact);

	fact_deinit(fact);
	free(fact);
}

/**
  Parses a single fact from a line formatted as follows:

  full.fact.name=value\n
 */
struct fact *fact_parse(const char *line)
{
	assert(line);

	struct fact *fact;
	char *buf, *name, *value;
	char *stp; /* string traversal pointer */

	buf = strdup(line);

	stp = buf;
	for (name = stp; *stp && *stp != '='; stp++)
		;
	*stp++ = '\0';
	for (value = stp; *stp && *stp != '\n'; stp++)
		;
	*stp = '\0';

	fact = fact_new(name, value);
	free(buf);

	return fact;
}

int fact_read(struct list *facts, FILE *io)
{
	assert(facts);
	assert(io);

	struct fact *fact;
	char buf[8192] = {0};
	int nfacts = 0;

	while (!feof(io)) {
		errno = 0;
		if (!fgets(buf, 8192, io)) {
			if (errno == 0) {
				break;
			}
			return -1;
		}

		/* FIXME: how do we test for long lines properly? */
		fact = fact_parse(buf);
		if (fact) {
			list_add_tail(&fact->facts, facts);
			nfacts++;
		}
	}

	return nfacts;
}
