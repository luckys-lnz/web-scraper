#ifndef SCRAPER_H
#define SCRAPER_H

#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "thread_pool.h"
#include "stats.h"  // Include stats.h for statistics types
#include <hiredis/hiredis.h>
#include "types.h"
#include "content_analyzer.h"

// Struct for storing fetched HTML data
struct Memory {
  char *response;
  size_t size;
};

// Function prototypes
void extract_title(const char *html);
void extract_meta(const char *html);
void extract_hrefs(const char *html, const char *base_url);
int is_allowed_by_robots(const char *url);
void split_url(const char *url, char *base_url, char *target_path);

// URL processing functions
void init_scraper_pool(int thread_count);
void cleanup_scraper_pool();

// Initialize the scraper with CURL and Redis connections
// Returns 0 on success, -1 on failure
int init_scraper(void);

// Process a URL by fetching its content and storing in Redis
// Returns 0 on success, -1 on failure
int process_url(const char *url);

// Clean up resources (CURL and Redis connections)
void cleanup_scraper(void);

// AI-powered content analysis functions
content_analysis_t *analyze_url_content(const char *url);
trend_data_t **get_trending_topics(int limit);

// Configuration functions
void set_scraper_config(scraper_config_t *config);
scraper_config_t *get_scraper_config(void);

#endif // SCRAPER_H
