#include "cache.h"
#include "logger.h"
#include "redis_helper.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <hiredis/hiredis.h>

#define CACHE_PREFIX "cache:"
#define MAX_CACHE_SIZE 1000000  // 1MB
#define MAX_RETRIES 3
#define RETRY_DELAY 1  // seconds
#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379

// Helper function to create cache key
static char *create_cache_key(const char *prefix, const char *url) {
    size_t key_len = strlen(prefix) + strlen(url) + 1;
    char *key = malloc(key_len);
    if (!key) return NULL;
    snprintf(key, key_len, "%s%s", prefix, url);
    return key;
}

// Initialize cache
int cache_init(redisContext *ctx) {
    if (!ctx) {
        LOG_ERROR("Redis context is NULL");
        return 0;
    }

    // Verify Redis is working by setting up initial cache structures
    pthread_mutex_lock(&redis_mutex);
    
    // Test cache write with retry
    redisReply *reply = NULL;
    int retry_count = 0;
    while (retry_count < MAX_RETRIES) {
        reply = redisCommand(ctx, "SET %stest test_value", CACHE_PREFIX);
        if (reply && reply->type == REDIS_REPLY_STATUS) {
            break;
        }
        if (reply) {
            freeReplyObject(reply);
            reply = NULL;
        }
        LOG_WARNING("Cache write test failed, retrying (%d/%d)...", 
                   retry_count + 1, MAX_RETRIES);
        sleep(RETRY_DELAY);
        retry_count++;
    }

    if (!reply || reply->type != REDIS_REPLY_STATUS) {
        LOG_ERROR("Failed to write to Redis cache after %d attempts", MAX_RETRIES);
        pthread_mutex_unlock(&redis_mutex);
        return 0;
    }
    freeReplyObject(reply);

    // Test cache read
    reply = redisCommand(ctx, "GET %stest", CACHE_PREFIX);
    if (!reply || reply->type != REDIS_REPLY_STRING || 
        strcmp(reply->str, "test_value") != 0) {
        LOG_ERROR("Failed to read from Redis cache");
        pthread_mutex_unlock(&redis_mutex);
        return 0;
    }
    freeReplyObject(reply);

    // Clean up test key
    reply = redisCommand(ctx, "DEL %stest", CACHE_PREFIX);
    if (!reply || reply->type != REDIS_REPLY_INTEGER) {
        LOG_ERROR("Failed to clean up test key");
        pthread_mutex_unlock(&redis_mutex);
        return 0;
    }
    freeReplyObject(reply);

    pthread_mutex_unlock(&redis_mutex);
    LOG_INFO("Cache initialized successfully");
    return 1;
}

// Store content in cache
int cache_store_content(redisContext *ctx, const char *url, const char *content, size_t content_size,
                       const char *content_type, int status_code) {
    if (!ctx || !url || !content) {
        return 0;
    }

    char key[256];
    snprintf(key, sizeof(key), "%s%s", CACHE_PREFIX, url);

    pthread_mutex_lock(&redis_mutex);
    redisReply *reply = redisCommand(ctx, "HMSET %s content %b type %s status %d",
                                   key, content, content_size,
                                   content_type, status_code);
    pthread_mutex_unlock(&redis_mutex);

    if (!reply) {
        return 0;
    }

    int result = reply->type == REDIS_REPLY_STATUS;
    freeReplyObject(reply);
    return result;
}

// Store metadata in cache
int cache_store_metadata(redisContext *ctx, const char *url, const cached_metadata_t *metadata) {
    if (!ctx || !url || !metadata) {
        LOG_ERROR("Invalid parameters for cache_store_metadata");
        return -1;
    }

    char *key = create_cache_key(CACHE_META_PREFIX, url);
    if (!key) {
        LOG_ERROR("Failed to create metadata cache key");
        return -1;
    }

    redisReply *reply = redisCommand(ctx,
        "HMSET %s title %s description %s keywords %s author %s last_modified %ld",
        key,
        metadata->title ? metadata->title : "",
        metadata->description ? metadata->description : "",
        metadata->keywords ? metadata->keywords : "",
        metadata->author ? metadata->author : "",
        (long)metadata->last_modified);

    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR("Failed to store metadata in cache: %s",
                 reply ? reply->str : "Unknown error");
        freeReplyObject(reply);
        free(key);
        return -1;
    }

    // Set TTL
    redisReply *ttl_reply = redisCommand(ctx, "EXPIRE %s %d", key, CACHE_TTL);
    if (!ttl_reply || ttl_reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR("Failed to set metadata cache TTL: %s",
                 ttl_reply ? ttl_reply->str : "Unknown error");
    }
    freeReplyObject(ttl_reply);
    freeReplyObject(reply);
    free(key);
    return 0;
}

// Retrieve content from cache
int cache_get_content(redisContext *ctx, const char *url, char **content, size_t *content_size,
                     char **content_type, int *status_code) {
    if (!ctx || !url || !content || !content_size || 
        !content_type || !status_code) {
        return 0;
    }

    char key[256];
    snprintf(key, sizeof(key), "%s%s", CACHE_PREFIX, url);

    pthread_mutex_lock(&redis_mutex);
    redisReply *reply = redisCommand(ctx, "HMGET %s content type status", key);
    pthread_mutex_unlock(&redis_mutex);

    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements != 3) {
        if (reply) {
            freeReplyObject(reply);
        }
        return 0;
    }

    // Initialize output parameters
    *content = NULL;
    *content_size = 0;
    *content_type = NULL;
    *status_code = 0;

    // Get content
    if (reply->element[0] && reply->element[0]->type == REDIS_REPLY_STRING) {
        *content_size = reply->element[0]->len;
        *content = malloc(*content_size + 1);
        if (*content) {
            memcpy(*content, reply->element[0]->str, *content_size);
            (*content)[*content_size] = '\0';
        }
    }

    // Get content type
    if (reply->element[1] && reply->element[1]->type == REDIS_REPLY_STRING) {
        *content_type = strdup(reply->element[1]->str);
    }

    // Get status code
    if (reply->element[2] && reply->element[2]->type == REDIS_REPLY_STRING) {
        *status_code = atoi(reply->element[2]->str);
    }

    freeReplyObject(reply);
    return 1;
}

// Retrieve metadata from cache
cached_metadata_t *cache_get_metadata(redisContext *ctx, const char *url) {
    if (!ctx || !url) {
        LOG_ERROR("Invalid parameters for cache_get_metadata");
        return NULL;
    }

    char *key = create_cache_key(CACHE_META_PREFIX, url);
    if (!key) {
        LOG_ERROR("Failed to create metadata cache key");
        return NULL;
    }

    redisReply *reply = redisCommand(ctx, "HGETALL %s", key);
    free(key);

    if (!reply || reply->type == REDIS_REPLY_ERROR || reply->elements == 0) {
        LOG_DEBUG("Metadata not found in cache for URL: %s", url);
        freeReplyObject(reply);
        return NULL;
    }

    cached_metadata_t *metadata = malloc(sizeof(cached_metadata_t));
    if (!metadata) {
        LOG_ERROR("Failed to allocate memory for cached metadata");
        freeReplyObject(reply);
        return NULL;
    }

    // Initialize metadata structure
    metadata->title = NULL;
    metadata->description = NULL;
    metadata->keywords = NULL;
    metadata->author = NULL;
    metadata->last_modified = 0;

    // Parse Redis reply
    for (size_t i = 0; i < reply->elements; i += 2) {
        const char *field = reply->element[i]->str;
        const char *value = reply->element[i + 1]->str;

        if (strcmp(field, "title") == 0) {
            metadata->title = strdup(value);
        } else if (strcmp(field, "description") == 0) {
            metadata->description = strdup(value);
        } else if (strcmp(field, "keywords") == 0) {
            metadata->keywords = strdup(value);
        } else if (strcmp(field, "author") == 0) {
            metadata->author = strdup(value);
        } else if (strcmp(field, "last_modified") == 0) {
            metadata->last_modified = atol(value);
        }
    }

    freeReplyObject(reply);
    return metadata;
}

// Check if URL is cached
int cache_has_url(redisContext *ctx, const char *url) {
    if (!ctx || !url) {
        LOG_ERROR("Invalid parameters for cache_has_url");
        return 0;
    }

    char *key = create_cache_key(CACHE_PREFIX, url);
    if (!key) {
        LOG_ERROR("Failed to create cache key");
        return 0;
    }

    redisReply *reply = redisCommand(ctx, "EXISTS %s", key);
    free(key);

    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR("Failed to check cache existence: %s",
                 reply ? reply->str : "Unknown error");
        freeReplyObject(reply);
        return 0;
    }

    int exists = reply->integer;
    freeReplyObject(reply);
    return exists;
}

// Clear cache for a URL
int cache_clear_url(redisContext *ctx, const char *url) {
    if (!ctx || !url) {
        LOG_ERROR("Invalid parameters for cache_clear_url");
        return -1;
    }

    char *content_key = create_cache_key(CACHE_PREFIX, url);
    char *meta_key = create_cache_key(CACHE_META_PREFIX, url);
    if (!content_key || !meta_key) {
        LOG_ERROR("Failed to create cache keys");
        free(content_key);
        free(meta_key);
        return -1;
    }

    redisReply *reply = redisCommand(ctx, "DEL %s %s", content_key, meta_key);
    free(content_key);
    free(meta_key);

    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR("Failed to clear cache: %s", reply ? reply->str : "Unknown error");
        freeReplyObject(reply);
        return -1;
    }

    freeReplyObject(reply);
    return 0;
}

// Cleanup cache
void cache_cleanup(void) {
    // No cleanup needed as Redis handles cleanup automatically
} 