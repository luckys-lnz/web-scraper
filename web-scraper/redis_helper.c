#include "redis_helper.h"
#include "logger.h"
#include "robots_parser.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <hiredis/hiredis.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define VISITED_SET "visited_urls"
#define URL_QUEUE "url_queue"
#define MAX_RETRIES 3
#define RETRY_DELAY 1 // seconds

// Define Redis context and mutex as global variables
redisContext *redis_ctx = NULL;
pthread_mutex_t redis_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
int is_redis_initialized(void);
redisReply *execute_redis_command(const char *format, ...);

typedef struct {
  const char **urls;
  int count;
  int errors;
} bulk_operation_data;

// Get Redis context
redisContext *get_redis_context(void) {
    if (!redis_ctx || redis_ctx->err) {
        LOG_DEBUG("Redis context is invalid, attempting to reconnect");
        if (redis_ctx) {
            redisFree(redis_ctx);
            redis_ctx = NULL;
        }
        if (!init_redis(REDIS_HOST, REDIS_PORT)) {
            LOG_ERROR("Failed to reconnect to Redis");
            return NULL;
        }
    }
    return redis_ctx;
}

// Helper function to check if Redis is initialized
int is_redis_initialized(void) {
    redisContext *ctx = get_redis_context();
    if (!ctx) {
        LOG_DEBUG("Redis context is NULL");
        return 0;
    }
    
    // Test the connection with a simple PING
    redisReply *reply = redisCommand(ctx, "PING");
    if (!reply) {
        LOG_ERROR("Redis connection test failed");
        return 0;
    }
    freeReplyObject(reply);
    return 1;
}

// Helper function to execute Redis commands with retries
redisReply *execute_redis_command(const char *format, ...) {
  if (!is_redis_initialized()) {
    return NULL;
  }

  redisReply *reply = NULL;
  for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
    va_list args;
    va_start(args, format);
    reply = redisvCommand(redis_ctx, format, args);
    va_end(args);

    if (reply) {
      if (reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR("Redis error reply: %s", reply->str);
        freeReplyObject(reply);
        reply = NULL;
      } else {
        break;
      }
    } else {
      LOG_WARNING("Redis command failed (attempt %d/%d): %s", attempt + 1,
                  MAX_RETRIES, redis_ctx->errstr);
      sleep(RETRY_DELAY);
    }
  }

  if (!reply) {
    LOG_ERROR("Redis command ultimately failed after %d attempts", MAX_RETRIES);
  }

  return reply;
}

// Check if Redis is installed
static int is_redis_installed(void) {
  // Check if redis-server exists in PATH
  if (system("which redis-server > /dev/null 2>&1") == 0) {
    return 1;
  }

  // Check common installation paths
  const char *paths[] = {"/usr/bin/redis-server", "/usr/local/bin/redis-server",
                         "/opt/redis/bin/redis-server", NULL};

  for (const char **path = paths; *path; path++) {
    struct stat st;
    if (stat(*path, &st) == 0 && S_ISREG(st.st_mode)) {
      return 1;
    }
  }

  return 0;
}

// Check if Redis is running
static int is_redis_running(void) {
  // Check systemd service
  if (system("systemctl is-active redis > /dev/null 2>&1") == 0) {
    return 1;
  }

  // Check process
  if (system("pgrep redis-server > /dev/null 2>&1") == 0) {
    return 1;
  }

  return 0;
}

// Initialize Redis connection
int init_redis(const char *host, int port) {
    // If already connected, verify the connection
    if (redis_ctx) {
        LOG_DEBUG("Redis already connected, verifying connection...");
        if (is_redis_initialized()) {
            LOG_INFO("Existing Redis connection is valid");
            return 1;
        }
        LOG_WARNING("Existing Redis connection is invalid, reconnecting...");
        close_redis();
    }

    // Check if Redis is installed and running
    LOG_INFO("Validating Redis installation and status...");
    if (!is_redis_installed()) {
        LOG_ERROR("Redis is not installed. Please install Redis.");
        return 0;
    }
    if (!is_redis_running()) {
        LOG_ERROR("Redis is not running. Please start the Redis server.");
        return 0;
    }

    // Connect to Redis
    LOG_INFO("Connecting to Redis at %s:%d", host, port);
    struct timeval timeout = {1, 500000}; // 1.5 seconds
    redis_ctx = redisConnectWithTimeout(host, port, timeout);
    if (!redis_ctx || redis_ctx->err) {
        LOG_ERROR("Redis connection failed: %s",
              redis_ctx ? redis_ctx->errstr : "Unknown error");
        if (redis_ctx) {
            redisFree(redis_ctx);
            redis_ctx = NULL;
        }
        return 0;
    }

    // Test connection with PING
    redisReply *reply = redisCommand(redis_ctx, "PING");
    if (!reply || reply->type != REDIS_REPLY_STATUS ||
        strcmp(reply->str, "PONG") != 0) {
        LOG_ERROR("Redis PING failed or returned invalid response");
        if (reply)
            freeReplyObject(reply);
        redisFree(redis_ctx);
        redis_ctx = NULL;
        return 0;
    }
    freeReplyObject(reply);

    LOG_INFO("Redis connection established successfully");
    return 1;
}

// Close Redis connection
void close_redis(void) {
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
  if (!is_redis_initialized()) {
    return 0;
  }

  pthread_mutex_lock(&redis_mutex);
  redisReply *reply =
      execute_redis_command("SISMEMBER %s %s", VISITED_SET, url);
  pthread_mutex_unlock(&redis_mutex);

  if (!reply) {
    return 0;
  }

  int result = reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
  freeReplyObject(reply);
  return result;
}

int mark_visited_bulk(const char **urls, int count) {
  if (!is_redis_initialized() || !urls || count <= 0) {
    return 0;
  }

  pthread_mutex_lock(&redis_mutex);
  redisReply *reply = execute_redis_command("MULTI");
  if (!reply) {
    pthread_mutex_unlock(&redis_mutex);
    return 0;
  }
  freeReplyObject(reply);

  for (int i = 0; i < count; i++) {
    reply = execute_redis_command("SADD %s %s", VISITED_SET, urls[i]);
    if (!reply) {
      execute_redis_command("DISCARD");
      pthread_mutex_unlock(&redis_mutex);
      return 0;
    }
    freeReplyObject(reply);
  }

  reply = execute_redis_command("EXEC");
  pthread_mutex_unlock(&redis_mutex);

  if (!reply) {
    return 0;
  }

  int result = reply->type == REDIS_REPLY_ARRAY;
  freeReplyObject(reply);
  return result;
}

/**
 * Fetches a URL from the Redis queue.
 * Returns NULL if the queue is empty.
 */
char *fetch_url_from_queue(void) {
  if (!is_redis_initialized()) {
    return NULL;
  }

  pthread_mutex_lock(&redis_mutex);
  redisReply *reply =
      execute_redis_command("ZRANGE %s 0 0 WITHSCORES", URL_QUEUE);
  pthread_mutex_unlock(&redis_mutex);

  if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
    if (reply) {
      freeReplyObject(reply);
    }
    return NULL;
  }

  char *url = strdup(reply->element[0]->str);
  // Remove the URL from the queue
  pthread_mutex_lock(&redis_mutex);
  redisReply *del_reply = execute_redis_command("ZREM %s %s", URL_QUEUE, url);
  pthread_mutex_unlock(&redis_mutex);
  if (del_reply) {
    freeReplyObject(del_reply);
  }

  if (reply) {
    freeReplyObject(reply);
  }
  return url;
}

// Push URL to queue with priority
int push_url_to_queue(const char *url, int priority) {
  if (!is_redis_initialized() || !url) {
    return 0;
  }

  pthread_mutex_lock(&redis_mutex);
  redisReply *reply =
      execute_redis_command("ZADD url_queue %d %s", priority, url);
  pthread_mutex_unlock(&redis_mutex);

  if (!reply) {
    return 0;
  }

  int result = reply->type == REDIS_REPLY_INTEGER;
  freeReplyObject(reply);
  return result;
}
