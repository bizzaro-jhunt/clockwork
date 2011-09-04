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

#ifndef _HASH_H
#define _HASH_H

#include <sys/types.h>

/** @file hash.h

  Hashes, or associative arrays, is a generalization of a primitive C
  array, where strings are used for keys instead of integers.  Just like
  an array, each key is associated with a single value.

  A new hash is allocated and initialized with a call to hash_free, and
  freed via hash_free.  hash_set and hash_get are used to set and get the
  value for a specific key, respectively.

  <h3>Examples</h3>

  The following example illustrates the following core principles:

    -# allocation and de-allocation
    -# value storage / retrieval
    -# uniqueness of values not necessary
    -# uniqueness of keys is enforced

  @verbatim
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
  @endverbatim

 */

/** @cond false */
struct hash_cursor {
	ssize_t l1, l2;
};

struct hash_list {
	char **keys;
	void **values;
	ssize_t len;
};
/** @endcond */

/**
  Hash table
 */
struct hash {
	/** Hash buckets (64), used for fast indexing.
	    Each hash_list can contain multiple keys and values. */
	struct hash_list entries[64];
};

/**
  Hash a string into an 8-bit value.

  This function takes a string of arbitrary length and calculates an 8-bit
  unsigned value predictably.  This value will range from 0-63, and can be
  used for fast storage and lookup of values.

  This specific hashing algorithm is based on Dan Bernstein's efficient
  djb2 hash algorithm: http://www.cse.yorku.ca/~oz/hash.html

  Per the definition of a hash, the same value is returned for two strings
  if those strings are themselves identical.  Collisions are of course
  possible, since the set of possible strings is far larger than the set of
  possible values.

  @param  s    Arbitrary-length character string to hash

  @returns an unsigned 8-bit value from 0-63, based on \a s.
 */
unsigned char H64(const char *s);

/**
  Allocate a new hash structure, on the heap.

  Memory allocated by this function should only be freed through a call to
  hash_free or hash_free_all.

  @returns A pointer to a dynamically allocated hash structure, or NULL if
           one could not be allocated.
 */
struct hash *hash_new(void);

/**
  Free the memory allocated to a hash structure.

  This function does not free the memory housing the values of
  the hash, since that is considered to be the responsibility
  of the calling code.  To free the values as well, look at
  hash_free_all.

  @param  h    Hash structure to free.
 */
void hash_free(struct hash *h);

/**
  Free all memory allocated to a hash structure.

  This function also frees the memory housing the hash values.
  If the calling code wishes to manage that memory, hash_free is
  a better alternative.

  @param  h    Hash structure to free.
 */
void hash_free_all(struct hash *h);

/**
  Retrieve the value stored in a hash for a given key.

  @note It is entirely possible to store NULL values in a hash, however
        the current implementation does not deal with this situation
        intelligently, since NULL is also the return value for 'key not
        found' errors.  Venture forth at your own peril.

  @param  h    Hash to search.
  @param  k    Key to look for.

  @returns a pointer to the value found, or NULL if \a k was not found
           to be a key in \a h.
 */
void* hash_get(const struct hash *h, const char *k);

/**
  Store a value in a hash, under the given key.

  @note It is entirely possible to store NULL values in a hash, however
        the current implementation does not deal with this situation
        intelligently, since NULL is also the return value for failed
        store operations.  Venture forth at your own peril.

  @param  h    Hash to store \a v in.
  @param  k    Key under which to store \a v.
  @param  v    Pointer to a value to store.

  @returns a pointer to v (non-NULL) if the value was stored in the hash
           successfully, or NULL on failure.
 */
void* hash_set(struct hash *h, const char *k, void *v);

/** @cond false */
void *hash_next(const struct hash *h, struct hash_cursor *c, char **key, void **val);
/** @endcond */

/**
  Iterate over a hash, similar to a for loop in Perl, or PHP's foreach.

  Keys and values are sequentially visited, although order is non-obvious.

  Context is required for keeping track of the current position in the hash,
  so the \a cursor parameter (a pointer to a hash_cursor structure) exists.
  The cursor itself will be re-initialized as part of the for loop's setup
  clause, so pre-initialization is not necessary.

  This is a macro and it is designed to be used with a block, like this:

  @verbatim
  // assume that h is a hash with some stuff in it.

  char *k; void *v;
  struct hash_cursor cursor;

  for_each_key_value(h, &cursor, k, v) {
      printf("h[%s] = %p\n", k, v);
  }
  @endverbatim

  @param  hash      Pointer to the hash to iterate over.
  @param  cursor    Pointer to a hash_cursor structure to contain state.
  @param  key       String pointer (char*) to point at the current key.
  @param  val       Pointer (void*) to point at the current value.

 */
#define for_each_key_value(hash, cursor, key, val) \
	for ((cursor)->l1 = 0, (cursor)->l2 = -1; \
	     hash_next((hash), (cursor), &(key), (void**)&(val)); )

#endif
