/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

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

#ifndef _LIB_GEAR_H
#define _LIB_GEAR_H

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h> /* for offsetof(3) macro */
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

/**
  Fast, structure-agnostic doubly-linked circular list.

  This implementation was adapted from the Linux kernel list_head
  code, in include/linux/list.h.  Mostly, I removed the register
  optimizations, and implemented most operations in terms of
  __list_splice.
 */
struct list {
	struct list *next;   /* the next node in the list */
	struct list *prev;   /* the previous node in the list */
};

/**
  Variable-length Character String
 */
struct string {
	size_t len;     /* length of string in characters */
	size_t bytes;   /* length of buffer (string->raw) in bytes */
	size_t blk;     /* block size for buffer expansion */

	char *raw;      /* the string buffer */
	char *p;        /* internal pointer to NULL-terminator of raw */
};

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

/**
  A String List

  A stringlist is a specialized data structure designed for
  efficient storage and management of a list of strings.  It
  wraps around the C representation as a NULL-terminated array
  of character pointers, and extends the idiom with memory
  management methods and "standard" string list manipulation
  routines.

  ### Basic Usage ############################################

  Stringlist allocation and de-allocation is performed through
  the @stringlist_new and @stringlist_free functions.  These two
  functions allocate memory from the heap, and then free that
  memory.

  <code>
  // allocate a new, empty stringlist
  struct stringlist *lst = stringlist_new(NULL);

  // do something interesting with the list...
  stringlist_add(fruit, "pear");
  stringlist_add(fruit, "apple");
  stringlist_add(fruit, "cantaloupe");

  // free memory allocated by stringlist_new and stringlist_add
  stringlist_free(lst);
  </code>

  The first argument to @stringlist_new is a C-style `char**`
  string array, and can be used to "seed" the stringlist with
  starting values.  For example, to deal with command-line
  arguments in main() as a stringlist:

  <code>
  int main(int argc, char **argv)
  {
    struct stringlist *args = stringlist_new(argv)

    // ....
  }
  </code>

  It is not possible to set up a stringlist allocated on the stack.
 */
struct stringlist {
	size_t   num;      /* number of actual strings */
	size_t   len;      /* number of memory slots for strings */
	char   **strings;  /* array of NULL-terminated strings */
};

#define SPLIT_NORMAL  0x00
#define SPLIT_GREEDY  0x01

/**
  Callback function signature for sort comparisons.

  An sl_comparator function takes two arguments (adjacent strings
  in a stringlist) and returns an integer less than or greater than
  zero if the first value should come before or after the second.
 */
typedef int (*sl_comparator)(const void*, const void*);

struct path {
	char     *buf;
	ssize_t   n;
	size_t    len;
};

#define LOG_LEVEL_ALL         8
#define LOG_LEVEL_DEBUG       7
#define LOG_LEVEL_INFO        6
#define LOG_LEVEL_NOTICE      5
#define LOG_LEVEL_WARNING     4
#define LOG_LEVEL_ERROR       3
#define LOG_LEVEL_CRITICAL    2
#define LOG_LEVEL_NONE        1


char* string(const char *fmt, ...);
struct string* string_new(const char *str, size_t block);
void string_free(struct string *s);

int string_append(struct string *s, const char *str);
int string_append1(struct string *s, char c);
int string_interpolate(char *buf, size_t len, const char *src, const struct hash *ctx);

unsigned char H64(const char *s);
struct hash *hash_new(void);
void hash_free(struct hash *h);
void hash_free_all(struct hash *h);
void* hash_get(const struct hash *h, const char *k);
void* hash_set(struct hash *h, const char *k, void *v);
void *hash_next(const struct hash *h, struct hash_cursor *c, char **key, void **val);
int hash_merge(struct hash *a, const struct hash *b);

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

char* pack(const char *prefix, const char *format, ...);
int unpack(const char *packed, const char *prefix, const char *format, ...);

#define for_each_string(l,i) for ((i)=0; (i)<(l)->num; (i)++)

int STRINGLIST_SORT_ASC(const void *a, const void *b);
int STRINGLIST_SORT_DESC(const void *a, const void *b);

struct stringlist* stringlist_new(char** src);
struct stringlist* stringlist_dup(struct stringlist *orig);
void stringlist_free(struct stringlist *list);
void stringlist_sort(struct stringlist *list, sl_comparator cmp);
void stringlist_uniq(struct stringlist *list);
int stringlist_search(const struct stringlist *list, const char *needle);
int stringlist_add(struct stringlist *list, const char *value);
int stringlist_add_all(struct stringlist *dst, const struct stringlist *src);
int stringlist_remove(struct stringlist *list, const char *value);
int stringlist_remove_all(struct stringlist *dst, struct stringlist *src);
struct stringlist* stringlist_intersect(const struct stringlist *a, const struct stringlist *b);
int stringlist_diff(struct stringlist *a, struct stringlist *b);
char* stringlist_join(struct stringlist *list, const char *delim);
struct stringlist* stringlist_split(const char *str, size_t len, const char *delim, int opt);

struct path* path_new(const char *path);
void path_free(struct path *path);
const char *path(struct path *path);
int path_canon(struct path *path);
int path_push(struct path *path);
int path_pop(struct path *path);

void log_init(const char *ident);
int log_set(int level);
int log_level(void);
const char* log_level_name(int level);

void CRITICAL(const char *format, ...);
void ERROR(const char *format, ...);
void WARNING(const char *format, ...);
void NOTICE(const char *format, ...);
void INFO(const char *format, ...);
void DEBUG(const char *format, ...);

/** Declare / initialize an empty list */
#define LIST(n) struct list n = { &(n), &(n) }

/** Initialize a list pointer */
#define list_init(n) ((n)->next = (n)->prev = (n))

/** Retrieve data node (type t) from list l, in member m */
#define list_node(l,t,m) ((t*)((char*)(l) - offsetof(t,m)))

/** Retrieve first data node (type t).  See list_node */
#define list_head(l,t,m) list_node((l)->next, t, m)

/** Retrieve last data node (type t).  See list_node */
#define list_tail(l,t,m) list_node((l)->prev, t, m)

/** Iterate over a list */
#define for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/** Iterate over a list (safely) */
#define for_each_safe(pos, tmp, head) \
	for (pos = (head)->next, tmp = pos->next; pos != (head); pos = tmp, tmp = pos->next)

/** Iterate over a list, accessing data nodes */
#define for_each_node(pos, head, memb) \
	for (pos = list_node((head)->next, typeof(*pos), memb); \
	     &pos->memb != (head);                              \
	     pos = list_node(pos->memb.next, typeof(*pos), memb))

/** Iterate over a list (safely), accessing data nodes */
#define for_each_node_safe(pos, tmp, head, memb) \
	for (pos = list_node((head)->next, typeof(*pos), memb),            \
	         tmp = list_node(pos->memb.next, typeof(*pos), memb);      \
	     &pos->memb != (head);                                         \
	     pos = tmp, tmp = list_node(tmp->memb.next, typeof(*tmp), memb))

/** Iterate backwards over a list */
#define for_each_r(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

/** Iterate backwards over a list (safely) */
#define for_each_safe_r(pos, tmp, head) \
	for (pos = (head)->prev, tmp = pos->prev; pos != (head); pos = tmp, tmp = pos->prev)

/** Iterate backwards over a list, accessing data nodes. */
#define for_each_node_r(pos, head, memb) \
	for (pos = list_node((head)->prev, typeof(*pos), memb); \
	     &pos->memb != (head);                              \
	     pos = list_node(pos->memb.prev, typeof(*pos), memb))

/** Iterate backwards over a list (safely), accessing data nodes. */
#define for_each_node_safe_r(pos, tmp, head, memb) \
	for (pos = list_node((head)->prev, typeof(*pos), memb),            \
	         tmp = list_node(pos->memb.prev, typeof(*pos), memb);      \
	     &pos->memb != (head);                                         \
	     pos = tmp, tmp = list_node(tmp->memb.prev, typeof(*tmp), memb))

/** Is list l empty? */
static inline int list_empty(const struct list *l)
{
	return l->next == l;
}

static inline void __list_splice(struct list *prev, struct list *next)
{
	prev->next = next;
	next->prev = prev;
}

/** add n at the head of l */
static inline void list_add_head(struct list *n, struct list *l)
{
	__list_splice(n, l->next);
	__list_splice(l, n);
}

/** add n at the tail of l */
static inline void list_add_tail(struct list *n, struct list *l)
{
	__list_splice(l->prev, n);
	__list_splice(n, l);
}

/** remove n from its list */
static inline void list_del(struct list *n)
{
	__list_splice(n->prev, n->next);
	list_init(n);
}

/* INTERNAL: replace o with n */
static inline void __list_replace(struct list *o, struct list *n)
{
	n->next = o->next;
	n->next->prev = n;
	n->prev = o->prev;
	n->prev->next = n;
}

/** Replace o with n in o's list; initialize o */
static inline void list_replace(struct list *o, struct list *n)
{
	__list_replace(o, n);
	list_init(o);
}

/** Move n to the head of l */
static inline void list_move_head(struct list *n, struct list *l)
{
	__list_splice(n->prev, n->next);
	list_add_head(n, l);
}

/** Move n to the tail of l */
static inline void list_move_tail(struct list *n, struct list *l)
{
	__list_splice(n->prev, n->next);
	list_add_tail(n, l);
}

/** Join tail onto the end of head */
static inline void list_join(struct list *head, struct list *tail)
{
	__list_splice(head->prev, tail->next);
	__list_splice(tail->prev, head);
	list_init(tail);
}

#endif
