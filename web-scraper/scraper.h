#ifndef SCRAPER_H
#define SCRAPER_H

#include <stddef.h>

// Struct for storing fetched HTML data
struct Memory {
  char *response;
  size_t size;
};

// Function prototypes
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
void extract_title(const char *html);
void extract_meta(const char *html);
void extract_hrefs(const char *html);
void fetch_url(const char *url, struct Memory *chunk);

#endif // SCRAPER_H
