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

#include "test.h"
#include "../src/mesh.h"

TESTS {
	subtest {
		filter_t *f = filter_new();
		isnt_null(f, "filter_new() allocated a new filter");
		filter_destroy(f);
	}

	subtest { /* parse - basic case */
		filter_t *f = filter_parse("this.fact=literalvalue");
		isnt_null(f, "filter_parse() allocated a new filter");

		is_string(f->fact, "this.fact", "parsed fact name from f=v");
		ok(f->match, "f=v is an affirmative match");
		is_string(f->literal, "literalvalue", "parsed fact value from f=v");
		is_null(f->regex, "pcre regex is null for literal comparison");

		filter_destroy(f);
	}

	subtest { /* parse - internal whitespace (common case) */
		filter_t *f = filter_parse("next.fact = whitespace");
		isnt_null(f, "parsed a filter from sample with whitespace");

		is_string(f->fact, "next.fact", "parsed fact name from 'f = v'");
		ok(f->match, "'f = v' is an affirmative match");
		is_string(f->literal, "whitespace", "parsed fact value from 'f = v'");
		is_null(f->regex, "pcre regex is null for literal filter 'f = v'");

		filter_destroy(f);
	}

	subtest { /* parse - lots of whitespace (pathological case) */
		filter_t *f = filter_parse("\t  to.ken\n\r\t=   some space is significant\r\n");
		isnt_null(f, "parsed a filter from pathological whitespace sample");

		is_string(f->fact, "to.ken", "parsed fact name from pathological whitespace");
		ok(f->match, "pathogical whitespace case is an affirmative match");
		is_string(f->literal, "some space is significant",
				"interior whitespace is okay, newline is ignored");
		is_null(f->regex, "pcre regex is null for literal filter");

		filter_destroy(f);
	}

	subtest { /* parse - negative literal */
		filter_t *f = filter_parse("this.fact != literal");
		isnt_null(f, "filter_parse() allocated a new filter");

		is_string(f->fact, "this.fact", "parsed fact name from f != v");
		ok(!f->match, "f != v is a negative match");
		is_string(f->literal, "literal", "parsed fact value from f != v");
		is_null(f->regex, "pcre regex is null for literal comparison");

		filter_destroy(f);
	}

	subtest { /* parse - regex */
		filter_t *f = filter_parse("sys.fqdn=/^web0\\d/");
		isnt_null(f, "parsed a regex filter");

		is_string(f->fact, "sys.fqdn", "parsed fact name from regex filter");
		is_null(f->literal, "no literal value in regex filter");
		isnt_null(f->regex, "built a pcre object for regex filter");
		ok(f->match, "f=/v/ is an affirmative regex match");

		filter_destroy(f);
	}

	subtest { /* match - literal value */
		hash_t *host1 = vmalloc(sizeof(hash_t));
		hash_set(host1, "sys.hostname", "host1");
		hash_set(host1, "sys.fqdn",     "host1.example.com");
		hash_set(host1, "sys.os",       "Linux");

		hash_t *host2 = vmalloc(sizeof(hash_t));
		hash_set(host2, "sys.hostname", "host2");
		hash_set(host2, "sys.fqdn",     "host2.example.com");
		hash_set(host2, "sys.os",       "Linux");

		hash_t *empty = vmalloc(sizeof(hash_t));

		filter_t *f = filter_parse("sys.hostname=host1");
		isnt_null(f, "got a filter for literal value testing");

		ok( filter_match(f, host1), "sys.hostname=host1 should match host1");
		ok(!filter_match(f, host2), "sys.hostname=host1 should not match host2");
		ok(!filter_match(f, empty), "no match for empty facts (ever)");

		filter_destroy(f);
		hash_done(host1, 0); free(host1);
		hash_done(host2, 0); free(host2);
		hash_done(empty, 0); free(empty);
	}

	subtest { /* match - negated literal */
		hash_t *host1 = vmalloc(sizeof(hash_t));
		hash_set(host1, "sys.hostname", "host1");
		hash_set(host1, "sys.fqdn",     "host1.example.com");
		hash_set(host1, "sys.os",       "Linux");

		hash_t *host2 = vmalloc(sizeof(hash_t));
		hash_set(host2, "sys.hostname", "host2");
		hash_set(host2, "sys.fqdn",     "host2.example.com");
		hash_set(host2, "sys.os",       "Linux");

		hash_t *empty = vmalloc(sizeof(hash_t));

		filter_t *f = filter_parse("sys.hostname != host1");
		isnt_null(f, "got a filter for negated literal value testing");

		ok(!filter_match(f, host1), "sys.hostname!=host1 should not match host1");
		ok( filter_match(f, host2), "sys.hostname!=host1 should match host2");
		ok(!filter_match(f, empty), "no match for empty facts in negated matches");

		filter_destroy(f);
		hash_done(host1, 0); free(host1);
		hash_done(host2, 0); free(host2);
		hash_done(empty, 0); free(empty);
	}

	subtest { /* match - regex */
		hash_t *host1 = vmalloc(sizeof(hash_t));
		hash_set(host1, "sys.hostname", "host1");
		hash_set(host1, "sys.fqdn",     "host1.example.com");
		hash_set(host1, "sys.os",       "Linux");

		hash_t *host2 = vmalloc(sizeof(hash_t));
		hash_set(host2, "sys.hostname", "host2");
		hash_set(host2, "sys.fqdn",     "host2.example.com");
		hash_set(host2, "sys.os",       "Linux");

		hash_t *empty = vmalloc(sizeof(hash_t));

		filter_t *f = filter_parse("sys.fqdn=/^host1/");
		isnt_null(f, "got a filter for regex testing");

		ok( filter_match(f, host1), "sys.fqdn = /^host1/ should match host1");
		ok(!filter_match(f, host2), "sys.fqdn = /^host1/ should not match host2");
		ok(!filter_match(f, empty), "no match for empty facts in negated matches");

		filter_destroy(f);
		hash_done(host1, 0); free(host1);
		hash_done(host2, 0); free(host2);
		hash_done(empty, 0); free(empty);
	}

	subtest { /* match - negated regex */
		hash_t *host1 = vmalloc(sizeof(hash_t));
		hash_set(host1, "sys.hostname", "host1");
		hash_set(host1, "sys.fqdn",     "host1.example.com");
		hash_set(host1, "sys.os",       "Linux");

		hash_t *host2 = vmalloc(sizeof(hash_t));
		hash_set(host2, "sys.hostname", "host2");
		hash_set(host2, "sys.fqdn",     "host2.example.com");
		hash_set(host2, "sys.os",       "Linux");

		hash_t *empty = vmalloc(sizeof(hash_t));

		filter_t *f = filter_parse("sys.fqdn != /^host1/");
		isnt_null(f, "got a filter for negated regex testing");

		ok(!filter_match(f, host1), "sys.fqdn = /^host1/ should not match host1");
		ok( filter_match(f, host2), "sys.fqdn = /^host1/ should match host2");
		ok(!filter_match(f, empty), "no match for empty facts in negated matches");

		filter_destroy(f);
		hash_done(host1, 0); free(host1);
		hash_done(host2, 0); free(host2);
		hash_done(empty, 0); free(empty);
	}

	subtest { /* match - regex is case-insensitive (always) */
		hash_t *host1 = vmalloc(sizeof(hash_t));
		hash_set(host1, "sys.hostname", "host1");
		hash_set(host1, "sys.fqdn",     "host1.example.com");
		hash_set(host1, "sys.os",       "Linux");

		hash_t *host2 = vmalloc(sizeof(hash_t));
		hash_set(host2, "sys.hostname", "host2");
		hash_set(host2, "sys.fqdn",     "host2.example.com");
		hash_set(host2, "sys.os",       "Linux");

		hash_t *empty = vmalloc(sizeof(hash_t));

		filter_t *f = filter_parse("sys.fqdn=/^HOST1/");
		isnt_null(f, "got a filter for case-insesntive regex testing");

		ok( filter_match(f, host1), "sys.fqdn = /^HOST1/ should match host1");
		ok(!filter_match(f, host2), "sys.fqdn = /^HOST1/ should not match host2");
		ok(!filter_match(f, empty), "no match for empty facts in negated matches");

		filter_destroy(f);
		hash_done(host1, 0); free(host1);
		hash_done(host2, 0); free(host2);
		hash_done(empty, 0); free(empty);
	}

	subtest { /* match all - mix-n-match regex, literal and negation */
		hash_t *host1 = vmalloc(sizeof(hash_t));
		hash_set(host1, "sys.hostname", "host1");
		hash_set(host1, "sys.fqdn",     "host1.example.com");
		hash_set(host1, "sys.os",       "Linux");

		hash_t *host2 = vmalloc(sizeof(hash_t));
		hash_set(host2, "sys.hostname", "host2");
		hash_set(host2, "sys.fqdn",     "host2.example.com");
		hash_set(host2, "sys.os",       "Linux");

		hash_t *empty = vmalloc(sizeof(hash_t));

		filter_t *f1 = filter_parse("sys.os != SunOS");
		filter_t *f2 = filter_parse("sys.fqdn = /example.com$/");
		filter_t *f3 = filter_parse("sys.hostname != /host[2468]/");

		LIST(all);
		isnt_null(f1, "f1 is a filter"); list_push(&all, &f1->l);
		isnt_null(f2, "f2 is a filter"); list_push(&all, &f2->l);
		isnt_null(f3, "f3 is a filter"); list_push(&all, &f3->l);

		ok( filter_matchall(&all, host1), "f1 && f2 && f3 should match host1");
		ok(!filter_matchall(&all, host2), "f1 && f2 && f3 should not match host2");
		ok(!filter_matchall(&all, empty), "missing fact value immediately fails match");

		LIST(nofilter);
		ok( filter_matchall(&nofilter, host1), "empty filter should match host1");
		ok( filter_matchall(&nofilter, host2), "empty filter should match host2");
		ok( filter_matchall(&nofilter, empty), "empty filter should match no facts");

		filter_destroy(f1);
		filter_destroy(f2);
		filter_destroy(f3);
		hash_done(host1, 0); free(host1);
		hash_done(host2, 0); free(host2);
		hash_done(empty, 0); free(empty);
	}

	subtest { /* parse list of filters */
		LIST(filters);

		filter_parseall(&filters, "sys.fqdn=/^host1/\nsys.os!=SunOS\nsys.os=Linux\n");
		is_int(list_len(&filters), 3, "parsed three filters");

		hash_t *host1 = vmalloc(sizeof(hash_t));
		hash_set(host1, "sys.hostname", "host1");
		hash_set(host1, "sys.fqdn",     "host1.example.com");
		hash_set(host1, "sys.os",       "Linux");

		hash_t *host2 = vmalloc(sizeof(hash_t));
		hash_set(host2, "sys.hostname", "host2");
		hash_set(host2, "sys.fqdn",     "host2.example.com");
		hash_set(host2, "sys.os",       "Linux");

		hash_t *empty = vmalloc(sizeof(hash_t));

		ok( filter_matchall(&filters, host1), "should match host1 and os Linux");
		ok(!filter_matchall(&filters, host2), "should not match host2");
		ok(!filter_matchall(&filters, empty), "missing fact value immediately fails match");

		filter_t *f, *tmp;
		for_each_object_safe(f, tmp, &filters, l)
			filter_destroy(f);

		hash_done(host1, 0); free(host1);
		hash_done(host2, 0); free(host2);
		hash_done(empty, 0); free(empty);
	}

	done_testing();
}
