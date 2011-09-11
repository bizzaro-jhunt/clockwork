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

#ifndef HASH_H
#define HASH_H

#include <sys/types.h>

struct hash_cursor {
	ssize_t l1, l2;
};

struct hash_list {
	char **keys;
	void **values;
	ssize_t len;
};

/**
  Unordered Hash

  Hashes, or associative arrays, are a generalization of a standard C
  array, where strings are used for keys instead of integers.  Just like
  an array, each key is associated with a single value.

  A new hash is allocated and initialized with a call to @hash_new, and
  freed via @hash_free.  @hash_set and @hash_get are used to set and get the
  value for a specific key, respectively.

  ### Examples ####################################

  The following example illustrates some core principles:

    - allocation and de-allocation
    - value storage / retrieval
    - uniqueness of values not necessary
    - uniqueness of keys is enforced

  <code>
  struct hash *h = hash_new();

  // populate hash h with colors of various fruits,
  // using the name of the fruit as a key.
  hash_set(h, "Apple",  "red");
  hash_set(h, "Banana", "yellow");
  hash_set(h, "Grape",  "green");
  hash_set(h, "Lemon",  "yellow");
  hash_set(h, "Grape",  "purple");

  // retrieve some values.
  printf("%s\n", hash_get(h, "Banana")); // 'yellow'
  printf("%s\n", hash_get(h, "Lemon"));  // 'yellow'
  printf("%s\n", hash_get(h, "Grape"));  // 'purple'

  hash_free(h);
  </code>

  ### Implementation Details ########################

  Internally, a hash is implemented as an array of 64 lists.
  As values are inserted, the key is converted to an integer
  between 0 and 63 (by the so-called "hashing function") and
  the original (key,value) pair is stored in the appropriate
  list.

  This enables the implementation to gracefully handle hash
  collisions, where two distinct keys hash to the same
  integral value.
 */
struct hash {
	struct hash_list entries[64];
};

unsigned char H64(const char *s);

struct hash *hash_new(void);
void hash_free(struct hash *h);
void hash_free_all(struct hash *h);
void* hash_get(const struct hash *h, const char *k);
void* hash_set(struct hash *h, const char *k, void *v);

void *hash_next(const struct hash *h, struct hash_cursor *c, char **key, void **val);

/**
  Iterate over $h

  This macro is similar to a for loop in Perl, or PHP's foreach.

  Keys and values are sequentially visited, although order is non-obvious.

  Context is required for keeping track of the current position in the hash,
  so the $cursor parameter (a pointer to a hash_cursor structure) exists.
  The cursor itself will be re-initialized as part of the for loop's setup
  clause, so pre-initialization is not necessary.

  This is a macro and it is designed to be used with a block, like this:

  <code>
  // assume that h is a hash with some stuff in it.

  char *k; void *v;
  struct hash_cursor cursor;

  for_each_key_value(h, &cursor, k, v) {
      printf("h[%s] = %p\n", k, v);
  }
  </code>
 */
#define for_each_key_value(hash, cursor, key, val) \
	for ((cursor)->l1 = 0, (cursor)->l2 = -1; \
	     hash_next((hash), (cursor), &(key), (void**)&(val)); )

#endif
