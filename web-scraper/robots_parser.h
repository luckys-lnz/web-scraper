#ifndef ROBOTS_PARSER_H
#define ROBOTS_PARSER_H

#include <stdbool.h>
#include <pthread.h>
#include <hiredis/hiredis.h>

extern pthread_mutex_t redis_mutex;
extern redisContext *redis_ctx;

/**
 * Extracts the domain from a URL.
 * 
 * @param url The URL to extract the domain from.
 * @return A newly allocated string containing the domain, or NULL on error.
 *         The caller is responsible for freeing the returned string.
 */
char *extract_domain(const char *url);

/**
 * Checks if crawling is allowed for a given URL based on `robots.txt`.
 */
int is_crawl_allowed(const char *base_url, const char *target_path);

/**
 * Fetches and stores `robots.txt` rules for a given domain.
 */
void fetch_robots_txt(const char *url);

#endif // ROBOTS_PARSER_H
