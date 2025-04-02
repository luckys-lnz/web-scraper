#ifndef ROBOTS_PARSER_H
#define ROBOTS_PARSER_H

#include <stdbool.h>

/**
 * Checks if crawling is allowed for a given URL based on `robots.txt`.
 */
int is_crawl_allowed(const char *base_url, const char *target_path);

/**
 * Fetches and stores `robots.txt` rules for a given domain.
 */
void fetch_robots_txt(const char *url);

#endif // ROBOTS_PARSER_H
