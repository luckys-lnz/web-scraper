#ifndef REDIS_HELPER_H
#define REDIS_HELPER_H

#include <hiredis/hiredis.h>

// Redis connection
void init_redis();
void close_redis();

// URL deduplication
int is_visited(const char *url);
void mark_visited_bulk(const char **urls, int count);
// URL queue management
char *fetch_url_from_queue();
void push_url_to_queue(const char *url, int priority);

#endif
