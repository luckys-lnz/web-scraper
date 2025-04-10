#include "redis_helper.h"
#include "robots_parser.h"
#include <hiredis/hiredis.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define VISITED_SET "visited_urls"
#define URL_QUEUE "url_queue"
#define MAX_RETRIES 5
#define RETRY_DELAY 1  // seconds

redisContext *redis_ctx = NULL; // Global Redis context

/**
 * Initializes the Redis connection.
 */
int init_redis() {
  int retries = 0;
  
  while (retries < MAX_RETRIES) {
    // Connect to Redis server
    redis_ctx = redisConnect(REDIS_HOST, REDIS_PORT);
    if (redis_ctx == NULL || redis_ctx->err) {
      if (redis_ctx) {
        fprintf(stderr, "Redis connection error: %s\n", redis_ctx->errstr);
        redisFree(redis_ctx);
        redis_ctx = NULL;
      } else {
        fprintf(stderr, "Redis connection error: can't allocate redis context\n");
      }
      
      retries++;
      if (retries < MAX_RETRIES) {
        fprintf(stderr, "Retrying in %d seconds... (attempt %d/%d)\n", 
                RETRY_DELAY, retries + 1, MAX_RETRIES);
        sleep(RETRY_DELAY);
        continue;
      }
      return -1;
    }

    // Test the connection
    redisReply *reply = redisCommand(redis_ctx, "PING");
    if (reply == NULL) {
      fprintf(stderr, "Redis PING failed\n");
      redisFree(redis_ctx);
      redis_ctx = NULL;
      retries++;
      if (retries < MAX_RETRIES) {
        fprintf(stderr, "Retrying in %d seconds... (attempt %d/%d)\n", 
                RETRY_DELAY, retries + 1, MAX_RETRIES);
        sleep(RETRY_DELAY);
        continue;
      }
      return -1;
    }
    
    if (reply->type == REDIS_REPLY_ERROR) {
      fprintf(stderr, "Redis PING error: %s\n", reply->str);
      freeReplyObject(reply);
      redisFree(redis_ctx);
      redis_ctx = NULL;
      retries++;
      if (retries < MAX_RETRIES) {
        fprintf(stderr, "Retrying in %d seconds... (attempt %d/%d)\n", 
                RETRY_DELAY, retries + 1, MAX_RETRIES);
        sleep(RETRY_DELAY);
        continue;
      }
      return -1;
    }
    
    freeReplyObject(reply);

    // Clear any existing keys to avoid type conflicts
    reply = redisCommand(redis_ctx, "DEL %s %s", VISITED_SET, URL_QUEUE);
    if (reply) {
      if (reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "Redis error clearing keys: %s\n", reply->str);
        freeReplyObject(reply);
        redisFree(redis_ctx);
        redis_ctx = NULL;
        return -1;
      }
      freeReplyObject(reply);
    }

    return 0;
  }
  
  return -1;
}

/**
 * Closes the Redis connection.
 */
void close_redis() {
  if (redis_ctx) {
    redisFree(redis_ctx);
    redis_ctx = NULL;
  }
}

/**
 * Checks if a URL has already been visited.
 * Returns 1 if visited, 0 otherwise.
 */
int is_visited(const char *url) {
  if (!redis_ctx) {
    fprintf(stderr, "Redis not initialized\n");
    return 0;
  }

  pthread_mutex_lock(&redis_mutex);
  redisReply *reply = redisCommand(redis_ctx, "SISMEMBER %s %s", VISITED_SET, url);
  if (!reply) {
    fprintf(stderr, "Redis command failed: SISMEMBER\n");
    pthread_mutex_unlock(&redis_mutex);
    return 0;
  }

  if (reply->type == REDIS_REPLY_ERROR) {
    fprintf(stderr, "Redis error: %s\n", reply->str);
    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return 0;
  }

  int visited = reply->integer;
  freeReplyObject(reply);
  pthread_mutex_unlock(&redis_mutex);
  return visited;
}

/**
 * Marks a URL as visited.
 */
void mark_visited_bulk(const char **urls, int count) {
  if (!redis_ctx) {
    fprintf(stderr, "Redis not initialized\n");
    return;
  }

  pthread_mutex_lock(&redis_mutex);

  for (int i = 0; i < count; i++) {
    redisAppendCommand(redis_ctx, "SADD %s %s", VISITED_SET, urls[i]);
  }

  for (int i = 0; i < count; i++) {
    redisReply *reply;
    if (redisGetReply(redis_ctx, (void **)&reply) != REDIS_OK) {
      fprintf(stderr, "Redis command failed: SADD\n");
      continue;
    }
    if (reply->type == REDIS_REPLY_ERROR) {
      fprintf(stderr, "Redis error: %s\n", reply->str);
    }
    freeReplyObject(reply);
  }

  pthread_mutex_unlock(&redis_mutex);
}

/**
 * Fetches a URL from the Redis queue.
 * Returns NULL if the queue is empty.
 */
char *fetch_url_from_queue() {
  if (!redis_ctx) {
    fprintf(stderr, "Redis not initialized\n");
    return NULL;
  }

  pthread_mutex_lock(&redis_mutex);
  
  // Fetch the highest priority URL
  redisReply *reply = redisCommand(redis_ctx, "ZRANGE %s 0 0 WITHSCORES", URL_QUEUE);
  if (!reply) {
    fprintf(stderr, "Redis command failed: ZRANGE\n");
    pthread_mutex_unlock(&redis_mutex);
    return NULL;
  }

  if (reply->type == REDIS_REPLY_ERROR) {
    fprintf(stderr, "Redis error: %s\n", reply->str);
    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return NULL;
  }

  if (reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return NULL;
  }

  char *url = strdup(reply->element[0]->str);
  freeReplyObject(reply);

  // Remove the URL from the queue after fetching
  reply = redisCommand(redis_ctx, "ZREM %s %s", URL_QUEUE, url);
  if (reply) {
    if (reply->type == REDIS_REPLY_ERROR) {
      fprintf(stderr, "Redis error: %s\n", reply->str);
    }
    freeReplyObject(reply);
  }

  pthread_mutex_unlock(&redis_mutex);
  return url;
}

/**
 * Pushes a new URL into the Redis priority queue if it hasn't been visited.
 */
void push_url_to_queue(const char *url, int priority) {
  if (!redis_ctx) {
    fprintf(stderr, "Redis not initialized\n");
    return;
  }

  pthread_mutex_lock(&redis_mutex);

  redisReply *reply = redisCommand(redis_ctx, "SISMEMBER %s %s", VISITED_SET, url);
  if (!reply) {
    fprintf(stderr, "Redis command failed: SISMEMBER\n");
    pthread_mutex_unlock(&redis_mutex);
    return;
  }

  if (reply->type == REDIS_REPLY_ERROR) {
    fprintf(stderr, "Redis error: %s\n", reply->str);
    freeReplyObject(reply);
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
  if (reply) {
    if (reply->type == REDIS_REPLY_ERROR) {
      fprintf(stderr, "Redis error: %s\n", reply->str);
    }
    freeReplyObject(reply);
  }

  pthread_mutex_unlock(&redis_mutex);
}
