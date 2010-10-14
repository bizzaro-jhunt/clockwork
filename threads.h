#ifndef _THREADS_H
#define _THREADS_H

#include <pthread.h>

#define THREAD_TYPE                pthread_t
#define THREAD_CREATE(id,ent,arg)  pthread_create(&(id), NULL, (ent), (arg))

#endif /* _THREADS_H */
