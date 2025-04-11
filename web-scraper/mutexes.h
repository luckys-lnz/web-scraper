#ifndef MUTEXES_H
#define MUTEXES_H

#include <pthread.h>

extern pthread_mutex_t redis_mutex;
extern pthread_mutex_t print_mutex;
extern pthread_mutex_t stats_mutex;

#endif // MUTEXES_H 