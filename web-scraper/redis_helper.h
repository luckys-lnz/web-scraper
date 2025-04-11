#ifndef REDIS_HELPER_H
#define REDIS_HELPER_H

#include <hiredis/hiredis.h>
#include <pthread.h>

// Global Redis context and mutex
extern redisContext *redis_ctx;
extern pthread_mutex_t redis_mutex;

// Initialize Redis connection
int init_redis(const char *host, int port);

// Close Redis connection
void close_redis(void);

// Check if URL has been visited
int is_visited(const char *url);

// Mark URL as visited
int mark_visited(const char *url);

// Mark multiple URLs as visited
int mark_visited_bulk(const char **urls, int count);

// Fetch URL from queue
char *fetch_url_from_queue(void);

// Push URL to queue with priority
int push_url_to_queue(const char *url, int priority);

// Execute Redis command with retries
redisReply *execute_redis_command(const char *format, ...);

// Check if Redis is initialized
int is_redis_initialized(void);

#endif // REDIS_HELPER_H
