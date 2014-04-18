/*
  Copyright 2011-2014 James Hunt <james@niftylogic.com>

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
#include "../src/template.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define TPL_FILE "t/tmp/scratch.tpl"

static int cat(const char *path, const char *what)
{
	int fd = -1;
	size_t len = 0;
	ssize_t nwritten = 0;

	if (!what || !path) return 0;

	fd = open(path, O_TRUNC|O_CREAT|O_WRONLY, 0644);
	if (fd < 0) return 0;

	len = strlen(what);
	do {
		nwritten = write(fd, what, len);
		if (nwritten < 0) return 0;

		what += nwritten;
		len  -= nwritten;
	} while (len > 0 && nwritten > 0);

	close(fd);
	return 1;
}

static char* render(const char *code)
{
	struct hash *v;
	struct template *t;
	char *final;

	v = hash_new();
	if (!v) return NULL;
	hash_set(v, "local.v1", "regression...");
	hash_set(v, "local.var1", "Var 1");

	if (!cat(TPL_FILE, code)) return NULL;

	t = template_create(TPL_FILE, v);
	if (!t) return NULL;

	final = template_render(t);
	hash_free(v);
	template_free(t);
	return final;
}

TESTS {
	subtest {
		struct template *t;
		char *actual;

		ok(cat(TPL_FILE, "static\n"), "created static template file");
		isnt_null(t = template_create(TPL_FILE, NULL), "template created");

		actual = template_render(t);
		isnt_null(t, "template rendered");
		is_string(actual, "static\n", "template rendered properly");

		free(actual);
		template_free(t);
	}

	subtest {
		struct hash *v;
		struct template *t;
		char *actual;

		isnt_null(v = hash_new(), "created vars hash");
		hash_set(v, "v.test1", "value1");
		hash_set(v, "v.test2", "value2");

		ok(cat(TPL_FILE, "v.test1 = <\%= v.test1 \%> and v.test2 = <\%= v.test2 \%>\n"),
				"created template file");
		isnt_null(t = template_create(TPL_FILE, v), "parsed template");
		isnt_null(actual = template_render(t), "template rendered");
		is_string(actual, "v.test1 = value1 and v.test2 = value2\n",
				"template rendered correctly");

		hash_free(v);
		free(actual);
		template_free(t);
	}

	subtest {
		struct hash *v;
		struct template *t;
		const char *expect = "v1: one\nv2: not two\n";
		char *actual;

		isnt_null(v = hash_new(), "created vars hash");
		hash_set(v, "v.v1", "1");
		hash_set(v, "v.v2", "42");

		ok(cat(TPL_FILE,
				"v1: <\% if v.v1 is \"1\" \%>one<\% else \%>not one<\% end \%>\n"
				"v2: <\% if v.v2 is not \"2\" \%>not two<\% else \%>two<\% end \%>\n"
			), "created template");
		isnt_null(t = template_create(TPL_FILE, v), "parsed template");
		isnt_null(actual = template_render(t), "template rendered");
		is_string(actual, "v1: one\nv2: not two\n", "template rendered correctly");

		hash_free(v);
		free(actual);
		template_free(t);

		isnt_null(v = hash_new(), "created vars hash");
		hash_set(v, "v.v1", "3");
		hash_set(v, "v.v2", "2");
		isnt_null(t = template_create(TPL_FILE, v), "parsed template");
		isnt_null(actual = template_render(t), "template rendered");
		is_string(actual, "v1: not one\nv2: two\n", "template rendered correctly");

		hash_free(v);
		free(actual);
		template_free(t);
	}

	subtest {
		char *s;
		is_string(
			s = render("<\% local.var1 = \"World!\" \%>Hello, <\%= local.var1 \%>\n"),
			"Hello, World!\n",
			"local variable expansion");
		free(s);
	}

	subtest {
		char *s;
		is_string(
			s = render("This is <\%= local.var1 = \"SO \" \%>great!\n"),
			"This is SO great!\n",
			"echo variable assignment");
		free(s);

		is_string(
			s = render("This is <\% local.var1 = \"NOT \" \%>great!\n"),
			"This is great!\n",
			"non-echo variable assignment");
		free(s);

		is_string(
			s = render("ref = <\%= local.var1 \%>\n"),
			"ref = Var 1\n",
			"echo variable de-reference");
		free(s);

		is_string(
			s = render("Non-echo refs<\% local.var \%>\n"),
			"Non-echo refs\n",
			"non-echo variable de-reference");
		free(s);


		is_string(
			s = render("Echo vals<\%= \" rock!\" \%>\n"),
			"Echo vals rock!\n",
			"echo string literal");
		free(s);

		is_string(
			s = render("Non-echo vals<\% \" REGRESS\" \%>\n"),
			"Non-echo vals\n",
			"non-echo string literal");
		free(s);
	}

	subtest {
		struct template *t = NULL;
		template_free(t);
		pass("template_free(NULL) doesn't segfault");
	}

	done_testing();
}
