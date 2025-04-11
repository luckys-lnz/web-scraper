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

// Struct for storing fetched HTML data
struct Memory {
  char *response;
  size_t size;
};

// URL processing task data
typedef struct {
    char *url;
    int priority;
} url_task_t;

// Function prototypes
void extract_title(const char *html);
void extract_meta(const char *html);
void extract_hrefs(const char *html, const char *base_url);
int is_allowed_by_robots(const char *url);

// URL processing functions
void *process_url(void *arg);
void init_scraper_pool(int thread_count);
void cleanup_scraper_pool();

#endif // SCRAPER_H
