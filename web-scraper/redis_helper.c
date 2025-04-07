#include "redis_helper.h"
#include <hiredis/hiredis.h>
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
redisContext *redis_ctx = NULL; // Global Redis context

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
  redisContext *ctx = redisConnect("127.0.0.1", 6379);
  if (!ctx || ctx->err) {
    fprintf(stderr, "Redis connection failed\n");
    return NULL;
  }

  // Fetch the highest priority URL
  redisReply *reply = redisCommand(ctx, "ZRANGE %s 0 0 WITHSCORES", URL_QUEUE);
  if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
    if (reply)
      freeReplyObject(reply);
    redisFree(ctx);
    return NULL;
  }

  char *url = strdup(reply->element[0]->str);
  freeReplyObject(reply);

  // Remove the URL from the queue after fetching
  reply = redisCommand(ctx, "ZREM %s %s", URL_QUEUE, url);
  if (reply)
    freeReplyObject(reply);

  redisFree(ctx);
  return url;
}

/**
 * Pushes a new URL into the Redis queue.
 */
void push_url_to_queue(const char *url, int priority) {
  redisReply *reply;

  // Check if URL has already been visited
  reply = redisCommand(redis_ctx, "SISMEMBER visited_urls %s", url);
  if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1) {
    freeReplyObject(reply);
    return; // Already visited, skip
  }
  freeReplyObject(reply);

  // Add to priority queue if not already present
  reply = redisCommand(redis_ctx, "ZADD url_queue %d %s", priority, url);
  freeReplyObject(reply);
}
