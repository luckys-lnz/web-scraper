#include "robots_parser.h"
#include "fetch_url.h"
#include "mutexes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

// Global variables
extern pthread_mutex_t redis_mutex;
extern redisContext *redis_ctx;

#define INITIAL_RULE_CAPACITY 16
#define MAX_RULE_LENGTH 2048
#define RULE_EXPIRY_SECONDS 86400 // 24 hours

// Error handling macros
#define CHECK_NULL(ptr, ret) \
    if (!(ptr)) { \
        fprintf(stderr, "Error: %s is NULL at %s:%d\n", #ptr, __FILE__, __LINE__); \
        return ret; \
    }

#define CHECK_REDIS_REPLY(reply, ret) \
    if (!(reply) || (reply)->type == REDIS_REPLY_ERROR) { \
        fprintf(stderr, "Redis error: %s\n", (reply) ? (reply)->str : "Unknown error"); \
        if (reply) freeReplyObject(reply); \
        return ret; \
    }

/**
 * Extracts the domain from a URL.
 */
char *extract_domain(const char *url) {
    CHECK_NULL(url, NULL);
    
    char *domain_start = strstr(url, "://");
    if (!domain_start)
        return NULL;
    domain_start += 3;

    char *domain_end = strchr(domain_start, '/');
    size_t domain_len =
        domain_end ? (size_t)(domain_end - domain_start) : strlen(domain_start);

    char *domain = malloc(domain_len + 1);
    if (!domain)
        return NULL;

    strncpy(domain, domain_start, domain_len);
    domain[domain_len] = '\0';
    return domain;
}

/**
 * Normalizes a path by removing query strings, fragments, and trailing slashes.
 * Returns a newly allocated string that must be freed by the caller.
 */
static char *normalize_path(const char *path) {
    CHECK_NULL(path, NULL);
    
    // Find the first occurrence of ? or #
    const char *query = strchr(path, '?');
    const char *fragment = strchr(path, '#');
    const char *end = path + strlen(path);
    
    // Determine where to truncate
    if (query && fragment) {
        end = query < fragment ? query : fragment;
    } else if (query) {
        end = query;
    } else if (fragment) {
        end = fragment;
    }
    
    // Remove trailing slashes
    while (end > path && *(end - 1) == '/') {
        end--;
    }
    
    // Allocate and copy the normalized path
    size_t len = end - path;
    if (len >= MAX_RULE_LENGTH) {
        fprintf(stderr, "Path too long: %zu characters\n", len);
        return NULL;
    }
    
    char *normalized = malloc(len + 1);
    if (!normalized) {
        fprintf(stderr, "Memory allocation failed: %s\n", strerror(errno));
        return NULL;
    }
    
    strncpy(normalized, path, len);
    normalized[len] = '\0';
    
    return normalized;
}

/**
 * Grows a dynamic array of strings.
 * Returns 0 on success, -1 on failure.
 */
static int grow_rule_array(char ***array, size_t *capacity) {
    size_t new_capacity = *capacity ? *capacity * 2 : INITIAL_RULE_CAPACITY;
    char **new_array = realloc(*array, new_capacity * sizeof(char *));
    
    if (!new_array) {
        fprintf(stderr, "Memory reallocation failed: %s\n", strerror(errno));
        return -1;
    }
    
    *array = new_array;
    *capacity = new_capacity;
    return 0;
}

/**
 * Comparison function for sorting rules.
 * Prioritizes longer paths to ensure more specific rules are checked first.
 */
static int rule_compare(const void *a, const void *b) {
    const char *rule_a = *(const char **)a;
    const char *rule_b = *(const char **)b;
    size_t len_a = strlen(rule_a);
    size_t len_b = strlen(rule_b);
    
    // First compare by length (longer paths first)
    if (len_a != len_b) {
        return len_b - len_a;
    }
    
    // Then by string content
    return strcmp(rule_a, rule_b);
}

/**
 * Fetches and prints `robots.txt` for a given domain.
 * 
 * @param url The URL of the website to fetch the robots.txt file for.
 * @return 1 if allowed, 0 otherwise.
 */
void fetch_robots_txt(const char *url) {
    CHECK_NULL(url, );
    
    char *domain = extract_domain(url);
    CHECK_NULL(domain, );
    
    char redis_key[256];
    int key_len = snprintf(redis_key, sizeof(redis_key), "robots:%s", domain);
    if (key_len < 0 || (size_t)key_len >= sizeof(redis_key)) {
        fprintf(stderr, "Redis key too long\n");
        free(domain);
        return;
    }
    
    // Check if rules are already cached and have correct type
    pthread_mutex_lock(&redis_mutex);
    
    // Check if allow key exists and has correct type
    redisReply *type_check = redisCommand(redis_ctx, "TYPE %s:allow", redis_key);
    if (type_check && type_check->type == REDIS_REPLY_STATUS) {
        if (strcmp(type_check->str, "list") != 0) {
            // Delete the key if it exists with wrong type
            redisReply *del_reply = redisCommand(redis_ctx, "DEL %s:allow", redis_key);
            freeReplyObject(del_reply);
        }
    }
    freeReplyObject(type_check);
    
    // Check if disallow key exists and has correct type
    type_check = redisCommand(redis_ctx, "TYPE %s:disallow", redis_key);
    if (type_check && type_check->type == REDIS_REPLY_STATUS) {
        if (strcmp(type_check->str, "list") != 0) {
            // Delete the key if it exists with wrong type
            redisReply *del_reply = redisCommand(redis_ctx, "DEL %s:disallow", redis_key);
            freeReplyObject(del_reply);
        }
    }
    freeReplyObject(type_check);
    
    // Check if rules are already cached
    redisReply *exists = redisCommand(redis_ctx, "EXISTS %s:allow", redis_key);
    CHECK_REDIS_REPLY(exists, );
    
    if (exists->integer > 0) {
        freeReplyObject(exists);
        pthread_mutex_unlock(&redis_mutex);
        free(domain);
        return;
    }
    freeReplyObject(exists);
    pthread_mutex_unlock(&redis_mutex);
    
    char robots_url[512];
    int url_len = snprintf(robots_url, sizeof(robots_url), "https://%s/robots.txt", domain);
    if (url_len < 0 || (size_t)url_len >= sizeof(robots_url)) {
        fprintf(stderr, "URL too long\n");
        free(domain);
        return;
    }
    
    struct Memory chunk = {NULL, 0};
    fetch_url(robots_url, &chunk);
    if (!chunk.response) {
        free(domain);
        return;
    }
    
    // Temporary arrays to store rules before sorting
    char **allow_rules = NULL;
    char **disallow_rules = NULL;
    size_t allow_count = 0;
    size_t disallow_count = 0;
    size_t allow_capacity = 0;
    size_t disallow_capacity = 0;
    
    char *line = strtok(chunk.response, "\n");
    while (line) {
        // Trim whitespace
        while (*line == ' ' || *line == '\t') line++;
        
        if (strncmp(line, "Disallow:", 9) == 0 || strncmp(line, "Allow:", 6) == 0) {
            const char *directive = strncmp(line, "Disallow:", 9) == 0 ? "Disallow:" : "Allow:";
            const char *path = line + (strncmp(directive, "Disallow:", 9) == 0 ? 9 : 6);
            
            // Trim whitespace from path
            while (*path == ' ' || *path == '\t') path++;
            
            if (*path) {
                char *normalized_path = normalize_path(path);
                if (normalized_path) {
                    if (strncmp(directive, "Allow:", 6) == 0) {
                        if (allow_count >= allow_capacity && grow_rule_array(&allow_rules, &allow_capacity) != 0) {
                            free(normalized_path);
                            goto cleanup;
                        }
                        allow_rules[allow_count++] = normalized_path;
                    } else {
                        if (disallow_count >= disallow_capacity && grow_rule_array(&disallow_rules, &disallow_capacity) != 0) {
                            free(normalized_path);
                            goto cleanup;
                        }
                        disallow_rules[disallow_count++] = normalized_path;
                    }
                }
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    // Sort the rules by length and content
    qsort(allow_rules, allow_count, sizeof(char *), rule_compare);
    qsort(disallow_rules, disallow_count, sizeof(char *), rule_compare);
    
    // Store sorted rules in Redis
    pthread_mutex_lock(&redis_mutex);
    
    // Use pipeline for better performance
    redisAppendCommand(redis_ctx, "MULTI");
    
    // Store allow rules
    for (size_t i = 0; i < allow_count; i++) {
        redisAppendCommand(redis_ctx, "RPUSH %s:allow %s", redis_key, allow_rules[i]);
    }
    
    // Store disallow rules
    for (size_t i = 0; i < disallow_count; i++) {
        redisAppendCommand(redis_ctx, "RPUSH %s:disallow %s", redis_key, disallow_rules[i]);
    }
    
    // Set expiration
    redisAppendCommand(redis_ctx, "EXPIRE %s:allow %d", redis_key, RULE_EXPIRY_SECONDS);
    redisAppendCommand(redis_ctx, "EXPIRE %s:disallow %d", redis_key, RULE_EXPIRY_SECONDS);
    
    redisAppendCommand(redis_ctx, "EXEC");
    
    // Execute pipeline
    redisReply *reply;
    for (size_t i = 0; i < allow_count + disallow_count + 3; i++) {
        redisGetReply(redis_ctx, (void **)&reply);
        freeReplyObject(reply);
    }
    
    pthread_mutex_unlock(&redis_mutex);
    
cleanup:
    // Free all allocated memory
    for (size_t i = 0; i < allow_count; i++) {
        free(allow_rules[i]);
    }
    for (size_t i = 0; i < disallow_count; i++) {
        free(disallow_rules[i]);
    }
    free(allow_rules);
    free(disallow_rules);
    free(chunk.response);
    free(domain);
}

/**
 * Checks if a path matches a robots.txt rule pattern.
 * Supports wildcards (*) at any position and exact matches.
 * 
 * @param path The path to check against the robots.txt rules.
 * @param rule The rule to match against the path.
 * @return 1 if the path matches the rule, 0 otherwise.
 */
static int path_matches_rule(const char *path, const char *rule) {
    CHECK_NULL(path, 0);
    CHECK_NULL(rule, 0);

    const char *wildcard = strchr(rule, '*');
    if (!wildcard) {
        return strcmp(path, rule) == 0;
    }

    // Handle the common case of wildcard at the end first
    if (rule[strlen(rule) - 1] == '*') {
        size_t rule_len = wildcard - rule;
        return strncmp(path, rule, rule_len) == 0;
    }

    // Handle wildcard at the start
    if (rule[0] == '*') {
        const char *suffix = rule + 1;
        size_t suffix_len = strlen(suffix);
        size_t path_len = strlen(path);
        
        if (path_len < suffix_len) return 0;
        return strcmp(path + (path_len - suffix_len), suffix) == 0;
    }

    // Handle wildcard in the middle
    char *rule_copy = strdup(rule);
    if (!rule_copy) {
        fprintf(stderr, "Memory allocation failed in path_matches_rule\n");
        return 0;
    }

    int result = 0;
    char *part1 = strtok(rule_copy, "*");
    char *part2 = strtok(NULL, "*");

    if (part1 && part2) {
        size_t part1_len = strlen(part1);
        if (strncmp(path, part1, part1_len) == 0) {
            const char *remaining_path = path + part1_len;
            result = strstr(remaining_path, part2) != NULL;
        }
    }

    free(rule_copy);
    return result;
}

/**
 * Checks if a path is allowed to be crawled based on `robots.txt`.
 * Returns 1 if allowed, 0 otherwise.
 * 
 * @param base_url The base URL of the website.
 * @param target_path The path to check against the robots.txt rules.
 * @return 1 if allowed, 0 otherwise.
 */
int is_crawl_allowed(const char *base_url, const char *target_path) {
    CHECK_NULL(base_url, 1);
    CHECK_NULL(target_path, 1);
    
    char *domain = extract_domain(base_url);
    CHECK_NULL(domain, 1);
    
    char redis_key[256];
    int key_len = snprintf(redis_key, sizeof(redis_key), "robots:%s", domain);
    if (key_len < 0 || (size_t)key_len >= sizeof(redis_key)) {
        fprintf(stderr, "Redis key too long\n");
        free(domain);
        return 1;
    }
    free(domain);
    
    char *normalized_path = normalize_path(target_path);
    CHECK_NULL(normalized_path, 1);
    
    pthread_mutex_lock(&redis_mutex);
    
    // Use pipeline for better performance
    redisAppendCommand(redis_ctx, "LRANGE %s:allow 0 -1", redis_key);
    redisAppendCommand(redis_ctx, "LRANGE %s:disallow 0 -1", redis_key);
    
    redisReply *allow_rules, *disallow_rules;
    redisGetReply(redis_ctx, (void **)&allow_rules);
    redisGetReply(redis_ctx, (void **)&disallow_rules);
    
    pthread_mutex_unlock(&redis_mutex);
    
    int result = 1; // Default to allowed
    
    // Check allow rules first
    if (allow_rules && allow_rules->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < allow_rules->elements; i++) {
            const char *rule = allow_rules->element[i]->str;
            if (path_matches_rule(normalized_path, rule)) {
                result = 1;
                goto cleanup;
            }
        }
    }
    
    // Check disallow rules
    if (disallow_rules && disallow_rules->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < disallow_rules->elements; i++) {
            const char *rule = disallow_rules->element[i]->str;
            if (path_matches_rule(normalized_path, rule)) {
                result = 0;
                goto cleanup;
            }
        }
    }
    
cleanup:
    freeReplyObject(allow_rules);
    freeReplyObject(disallow_rules);
    free(normalized_path);
    return result;
}

