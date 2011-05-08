#ifndef STRING_H
#define STRING_H

#include "clockwork.h"
#include "hash.h"

struct string {
	size_t len;
	size_t bytes;
	size_t blk;

	char *raw;
	char *p;
};

struct string* string_new(const char *str, size_t block);
void string_free(struct string *s);
int string_append(struct string *s, const char *str);
int string_append1(struct string *s, char c);

/**
  Interpolate variable references in a string

  Variable interpolation is performed according to the following rules:

  $([a-zA-Z][a-zA-Z0-9_-]*) is expanded to the string value stored in \a ctx
  under the key \1.  For example, in the string "My name is $name", "$name"
  is taken as a reference to the value stored in \a ctx under the key "name".

  Complex key names are also supported, using the format ${([^}]+)}.  To given
  another example, the string "My name is ${person.name}" contains a reference
  to the value stored in \a ctx under the key "person.name".
 */
int string_interpolate(char *buf, size_t len, const char *src, const struct hash *ctx);

#endif
