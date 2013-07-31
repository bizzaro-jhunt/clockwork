/*
  Copyright 2011-2013 James Hunt <james@jameshunt.us>

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

struct number {
	int value;
	struct list entry;
};

static struct number *number(int v)
{
	struct number *n = calloc(1, sizeof(struct number));
	if (n) {
		list_init(&n->entry);
		n->value = v;
	}
	return n;
}
#define NUMBER(x,v) struct number *x = number(v)

static int head_number(struct list *l)
{
	if (list_empty(l)) return 0;
	return list_head(l, struct number, entry)->value;
}

static int tail_number(struct list *l)
{
	if (list_empty(l)) return 0;
	return list_tail(l, struct number, entry)->value;
}

static int sum(struct list *l)
{
	struct number *n;
	int sum = 0;

	for_each_node(n, l, entry)
		sum += n->value;
	return sum;
}

/*************************************************************************/

int main(void) {
	test();

	subtest {
		NUMBER(n1, 4);
		NUMBER(n2, 8);
		NUMBER(n3, 15);
		LIST(l);

		ok(list_empty(&l), "LIST is empty by default");

		list_add_tail(&n2->entry, &l);
		is_int(head_number(&l),  8, "head([8])");
		is_int(tail_number(&l),  8, "tail([8])");

		list_add_head(&n1->entry, &l);
		is_int(head_number(&l),  4, "head([4, 8])");
		is_int(tail_number(&l),  8, "tail([4, 8])");

		list_add_tail(&n3->entry, &l);
		is_int(head_number(&l),  4, "head([4, 8, 15])");
		is_int(tail_number(&l), 15, "tail([4, 8, 15])");

		list_del(&n1->entry);
		is_int(head_number(&l),  8, "head([8, 15])");
		is_int(tail_number(&l), 15, "tail([8, 15])");

		list_add_tail(&n1->entry, &l);
		is_int(head_number(&l),  8, "head([8, 15, 4])");
		is_int(tail_number(&l),  4, "tail([8, 15, 4])");

		list_del(&n3->entry);
		is_int(head_number(&l),  8, "head([8, 4])");
		is_int(tail_number(&l),  4, "tail([8, 4])");

		list_del(&n2->entry);
		is_int(head_number(&l),  4, "head([4])");
		is_int(tail_number(&l),  4, "tail([4])");

		free(n1); free(n2); free(n3);
	}

	subtest {
		LIST(l);
		NUMBER(n4,   4); list_add_tail( &n4->entry, &l);
		NUMBER(n8,   8); list_add_tail( &n8->entry, &l);
		NUMBER(n15, 15); list_add_tail(&n15->entry, &l);
		NUMBER(n16, 16); list_add_tail(&n16->entry, &l);
		NUMBER(n23, 23); list_add_tail(&n23->entry, &l);
		NUMBER(n42, 42); list_add_tail(&n42->entry, &l);

		struct number *N;
		int sum = 0;

		for_each_node(N, &l, entry) sum += N->value;
		is_int(sum, 108, "for_each_node-sum of [4, 8, 15, 16, 23, 42]");

		sum = 0;
		for_each_node_r(N, &l, entry) sum += N->value;
		is_int(sum, 108, "for_each_node_r-sum of [4, 8, 15, 16, 23, 42]");

		free(n4); free(n8); free(n15); free(n16); free(n23); free(n42);
	}

	subtest {
		LIST(a);
		LIST(b);

		NUMBER(n4,   4); list_add_tail( &n4->entry, &a);
		NUMBER(n8,   8); list_add_tail( &n8->entry, &a);
		NUMBER(n15, 15); list_add_tail(&n15->entry, &a);

		NUMBER(n16, 16); list_add_tail(&n16->entry, &b);
		NUMBER(n23, 23); list_add_tail(&n23->entry, &b);
		NUMBER(n42, 42); list_add_tail(&n42->entry, &b);

		is_int(sum(&a), 27, "sum([ 4,  8, 15])");
		is_int(sum(&b), 81, "sum([16, 23, 42])");

		list_join(&a, &b);
		is_int(sum(&a), 108, "sum([4, 8, 15, 16, 23, 42])");
		is_int(head_number(&a),  4, "head([4, 8, 15, 16, 23, 42])");
		is_int(tail_number(&a), 42, "tail([4, 8, 15, 16, 23, 42])");
		ok(list_empty(&b), "b is empty post-join");

		free(n4); free(n8); free(n15); free(n16); free(n23); free(n42);
	}

	subtest {
		LIST(a);
		LIST(b);

		NUMBER(n4,   4); list_add_tail( &n4->entry, &a);
		NUMBER(n8,   8); list_add_tail( &n8->entry, &a);
		NUMBER(n15, 15); list_add_tail(&n15->entry, &a);

		NUMBER(n16, 16); list_add_tail(&n16->entry, &b);
		NUMBER(n23, 23); list_add_tail(&n23->entry, &b);
		NUMBER(n42, 42); list_add_tail(&n42->entry, &b);

		is_int(sum(&a), 27, "sum([ 4,  8, 15])");
		is_int(sum(&b), 81, "sum([16, 23, 42])");

		list_move_head(&n16->entry, &a);
		is_int(sum(&a), 43, "sum([16, 4,  8, 15])");
		is_int(sum(&b), 65, "sum([23, 42])");

		list_move_tail(&n4->entry, &b);
		is_int(sum(&a), 39, "sum([16,  8, 15])");
		is_int(sum(&b), 69, "sum([23, 42, 4])");

		free(n4); free(n8); free(n15); free(n16); free(n23); free(n42);
	}

	subtest {
		LIST(a);
		LIST(b);

		NUMBER(n4,   4); list_add_tail( &n4->entry, &a);
		NUMBER(n8,   8); list_add_tail( &n8->entry, &a);
		NUMBER(n15, 15); list_add_tail(&n15->entry, &a);

		NUMBER(n16, 16); list_add_tail(&n16->entry, &b);
		NUMBER(n23, 23); list_add_tail(&n23->entry, &b);
		NUMBER(n42, 42); list_add_tail(&n42->entry, &b);

		is_int(sum(&a), 27, "sum([ 4,  8, 15])");
		is_int(sum(&b), 81, "sum([16, 23, 42])");

		list_replace(&n4->entry, &n16->entry);
		is_int(sum(&a), 39, "sum([16, 8, 15])");

		list_replace(&n15->entry, &n42->entry);
		is_int(sum(&a), 66, "sum([16, 8, 42])");

		list_replace(&n8->entry, &n23->entry);
		is_int(sum(&a), 81, "sum([16, 23, 42])");

		free(n4); free(n8); free(n15); free(n16); free(n23); free(n42);
	}

	done_testing();
}
