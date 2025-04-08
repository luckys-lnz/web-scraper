#include "redis_helper.h"
#include <hiredis/hiredis.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
  int retries = 5;
  int delay_ms = 500;

  for (int i = 0; i < retries; ++i) {
    redis_ctx = redisConnect(REDIS_HOST, REDIS_PORT);
    if (redis_ctx && !redis_ctx->err) {
      return; // Success
    }

    if (redis_ctx) {
      fprintf(stderr, "Redis connection attempt %d failed: %s\n", i + 1,
              redis_ctx->errstr);
      redisFree(redis_ctx);
      redis_ctx = NULL;
    } else {
      fprintf(stderr, "Redis connection attempt %d failed: Unknown error\n",
              i + 1);
    }

    struct timespec req = {0};
    req.tv_sec = delay_ms / 1000;               // Convert ms to seconds
    req.tv_nsec = (delay_ms % 1000) * 1000000L; // Convert ms to nanoseconds
    nanosleep(&req, NULL);
  }

  fprintf(stderr, "Failed to connect to Redis after %d attempts.\n", retries);
  exit(EXIT_FAILURE);
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
void mark_visited_bulk(const char **urls, int count) {
  pthread_mutex_lock(&redis_mutex);

  for (int i = 0; i < count; i++) {
    redisAppendCommand(redis_ctx, "SADD %s %s", VISITED_SET, urls[i]);
  }

  for (int i = 0; i < count; i++) {
    redisReply *reply;
    redisGetReply(redis_ctx, (void **)&reply);
    freeReplyObject(reply);
  }

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
 * Pushes a new URL into the Redis priority queue if it hasn't been visited.
 */
void push_url_to_queue(const char *url, int priority) {
  pthread_mutex_lock(&redis_mutex);

  redisReply *reply =
      redisCommand(redis_ctx, "SISMEMBER %s %s", VISITED_SET, url);
  if (!reply) {
    fprintf(stderr, "Redis command failed: SISMEMBER\n");
    pthread_mutex_unlock(&redis_mutex);
    return;
  }

  if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1) {
    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return; // Already visited
  }
  freeReplyObject(reply);

  reply = redisCommand(redis_ctx, "ZADD %s %d %s", URL_QUEUE, priority, url);
  if (reply)
    freeReplyObject(reply);

  pthread_mutex_unlock(&redis_mutex);
}
