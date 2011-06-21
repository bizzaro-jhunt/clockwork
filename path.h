#ifndef _PATH_H
#define _PATH_H

typedef struct cw_path PATH;

PATH* path_new(const char *path);
void path_free(PATH *path);
const char *path(PATH *path);
int path_canon(PATH *path);
int path_push(PATH *path);
int path_pop(PATH *path);

#endif
