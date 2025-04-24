#ifndef ROBOTS_PARSER_H
#define ROBOTS_PARSER_H

#include <stdbool.h>
#include <pthread.h>
#include <hiredis/hiredis.h>
#include "rate_limiter.h"

extern pthread_mutex_t redis_mutex;

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
 * @param base_url The base URL of the website.
 * @param target_path The path to check against the robots.txt rules.
 * @param limiter The rate limiter containing the Redis context to use.
 * @return 1 if allowed, 0 otherwise.
 */
int is_crawl_allowed(const char *base_url, const char *target_path, rate_limiter_t *limiter);

/**
 * Fetches and stores `robots.txt` rules for a given domain.
 * @param url The URL of the website to fetch the robots.txt file for.
 * @param limiter The rate limiter containing the Redis context to use.
 */
void fetch_robots_txt(const char *url, rate_limiter_t *limiter);

#endif // ROBOTS_PARSER_H
