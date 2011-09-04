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

#include <assert.h>
#include <stdio.h>
#include <string.h>

static parser_branch* branch_new(char *fact, stringlist *values, unsigned char affirmative)
{
	parser_branch *branch;

	branch = malloc(sizeof(parser_branch));
	if (branch) {
		branch->fact = fact;
		branch->values = values;
		branch->affirmative = affirmative;
		branch->then = NULL;
		branch->otherwise = NULL;
	}

	return branch;
}

static void branch_free(parser_branch* branch)
{
	if (branch) {
		free(branch->fact);
		stringlist_free(branch->values);
	}
	free(branch);
}

static void branch_connect(parser_branch *branch, struct stree *then, struct stree *otherwise)
{
	if (branch->affirmative) {
		branch->then = then;
		branch->otherwise = otherwise;
	} else {
		branch->then = otherwise;
		branch->otherwise = then;
	}
}

static struct stree* branch_expand(struct manifest *m, parser_branch *branch)
{
	struct stree *top = NULL, *current = NULL;
	unsigned int i;

	for (i = 0; i < branch->values->num; i++) {
		current = manifest_new_stree(m, IF,
			strdup(branch->fact),
			strdup(branch->values->strings[i]));

		stree_add(current, branch->then);
		if (!top) {
			stree_add(current, branch->otherwise);
		} else {
			stree_add(current, top);
		}
		top = current;
	}
	return top;
}

static parser_map* map_new(char *fact, char *attr, stringlist *fact_values, stringlist *attr_values, char *default_value)
{
	parser_map *map;

	map = malloc(sizeof(parser_map));
	if (map) {
		map->fact = fact;
		map->attribute = attr;
		map->fact_values = fact_values;
		map->attr_values = attr_values;
		map->default_value = default_value;
	}

	return map;
}

static void map_free(parser_map *map)
{
	if (map) {
		free(map->fact);
		stringlist_free(map->fact_values);
		stringlist_free(map->attr_values);
	}
	free(map);
}

static struct stree* map_expand(struct manifest *m, parser_map *map)
{
	assert(map->fact_values);
	assert(map->attr_values);
	assert(map->fact_values->num == map->attr_values->num);

	struct stree *top = NULL, *current = NULL;
	unsigned int i;

	for (i = 0; i < map->fact_values->num; i++) {
		current = manifest_new_stree(m, IF,
			strdup(map->fact),
			strdup(map->fact_values->strings[i]));

		stree_add(current,
			manifest_new_stree(m, ATTR,
				strdup(map->attribute),
				strdup(map->attr_values->strings[i])));

		if (!top) {
			if (map->default_value) {
				stree_add(current, manifest_new_stree(m, ATTR, map->attribute, map->default_value));
			} else {
				stree_add(current, manifest_new_stree(m, NOOP, NULL, NULL));
			}
		} else {
			stree_add(current, top);
		}
		top = current;
	}

	return top;
}

