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

#endif // SCRAPER_H
