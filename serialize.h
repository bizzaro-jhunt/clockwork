#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <stdint.h>

typedef struct _serializer   serializer;
typedef struct _unserializer unserializer;

serializer* serializer_new(void);
void serializer_free(serializer *s);
int serializer_data(const serializer *s, char **dst, size_t *len);

int serializer_start(serializer *s);
int serializer_finish(serializer *s);
char* serializer_escape(const char *data);

int serializer_add_string(serializer *s, const char *data);
int serializer_add_character(serializer *s, unsigned char data);
int serializer_add_uint8(serializer *s, uint8_t data);
int serializer_add_uint16(serializer *s, uint16_t data);
int serializer_add_uint32(serializer *s, uint32_t data);
int serializer_add_int8(serializer *s, int8_t data);
int serializer_add_int16(serializer *s, int16_t data);
int serializer_add_int32(serializer *s, int32_t data);

unserializer* unserializer_new(const char *data, size_t len);
void unserializer_free(unserializer *u);

int unserializer_start(unserializer *u);
char* unserializer_unescape(const char *data);

int unserializer_next_string(unserializer *u, char **data, size_t *len);
int unserializer_next_character(unserializer *u, char *data);
int unserializer_next_uint8(unserializer *u, uint8_t *data);
int unserializer_next_uint16(unserializer *u, uint16_t *data);
int unserializer_next_uint32(unserializer *u, uint32_t *data);
int unserializer_next_int8(unserializer *u, int8_t *data);
int unserializer_next_int16(unserializer *u, int16_t *data);
int unserializer_next_int32(unserializer *u, int32_t *data);

#endif
