#include "redis_helper.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define VISITED_SET "visited_urls"
#define URL_QUEUE "url_queue"

static redisContext *redis = NULL;
static pthread_mutex_t redis_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Initializes the Redis connection.
 */
void init_redis() {
  redis = redisConnect(REDIS_HOST, REDIS_PORT);
  if (!redis || redis->err) {
    fprintf(stderr, "Redis connection failed: %s\n",
            redis ? redis->errstr : "Unknown error");
    exit(EXIT_FAILURE);
  }
}

/**
 * Closes the Redis connection.
 */
void close_redis() {
  if (redis) {
    redisFree(redis);
  }
}

int is_crawl_allowed(const char *base_url, const char *target_path) {
  // TODO: Implement actual robots.txt checking
  return 1; // Allow crawling for now (replace with actual logic)
}

/**
 * Checks if a URL has already been visited.
 * Returns 1 if visited, 0 otherwise.
 */
int is_visited(const char *url) {
  pthread_mutex_lock(&redis_mutex);
  redisReply *reply = redisCommand(redis, "SISMEMBER %s %s", VISITED_SET, url);
  int visited = reply->integer;
  freeReplyObject(reply);
  pthread_mutex_unlock(&redis_mutex);
  return visited;
}

/**
 * Marks a URL as visited.
 */
void mark_visited(const char *url) {
  pthread_mutex_lock(&redis_mutex);
  redisCommand(redis, "SADD %s %s", VISITED_SET, url);
  pthread_mutex_unlock(&redis_mutex);
}

/**
 * Fetches a URL from the Redis queue.
 * Returns NULL if the queue is empty.
 */
char *fetch_url_from_queue() {
  pthread_mutex_lock(&redis_mutex);
  redisReply *reply = redisCommand(redis, "LPOP %s", URL_QUEUE);

  char *url = NULL;
  if (reply && reply->type == REDIS_REPLY_STRING) {
    url = strdup(reply->str);
  }

  freeReplyObject(reply);
  pthread_mutex_unlock(&redis_mutex);
  return url;
}

/**
 * Pushes a new URL into the Redis queue.
 */
void push_url_to_queue(const char *url) {
  pthread_mutex_lock(&redis_mutex);
  redisCommand(redis, "RPUSH %s %s", URL_QUEUE, url);
  pthread_mutex_unlock(&redis_mutex);
}
