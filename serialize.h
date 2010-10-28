#ifndef SERIALIZE_H
#define SERIALIZE_H

typedef struct _serializer   serializer;
typedef struct _unserializer unserializer;

serializer* serializer_new(void);
int serializer_data(const serializer *s, char **dst, size_t *len);

int serializer_start(serializer *s);
int serializer_finish(serializer *s);
int serializer_add_string(serializer *s, const char *data);
int serializer_add_character(serializer *s, unsigned char data);
int serializer_add_unsigned_integer(serializer *s, unsigned long data);
int serializer_add_signed_integer(serializer *s, signed long data);

unserializer* unserializer_new(const char *data, size_t len);
int unserializer_start(unserializer *u);
int unserializer_get_next(unserializer *u, char **dst, size_t *len);

#endif
