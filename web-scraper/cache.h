#ifndef CACHE_H
#define CACHE_H

#include <hiredis/hiredis.h>
#include <pthread.h>

// Cache configuration
#define CACHE_TTL 86400  // 24 hours in seconds
#define CACHE_PREFIX "cache:"
#define CACHE_META_PREFIX "meta:"

// Structure for cached content
typedef struct {
    char *content;      // Raw HTML content
    size_t size;        // Content size in bytes
    time_t timestamp;   // Cache timestamp
    char *content_type; // Content type
    int status_code;    // HTTP status code
} cached_content_t;

// Structure for cached metadata
typedef struct {
    char *title;        // Page title
    char *description;  // Meta description
    char *keywords;     // Meta keywords
    char *author;       // Page author
    time_t last_modified; // Last modified time
} cached_metadata_t;

// Initialize cache
int cache_init(void);

// Store content in cache
int cache_store_content(const char *url, const char *content, size_t content_size, 
                       const char *content_type, int status_code);

// Store metadata in cache
int cache_store_metadata(const char *url, const cached_metadata_t *metadata);

// Get content from cache
int cache_get_content(const char *url, char **content, size_t *content_size, 
                     char **content_type, int *status_code);

// Retrieve metadata from cache
cached_metadata_t *cache_get_metadata(const char *url);

// Check if URL is cached
int cache_has_url(const char *url);

// Clear cache for a URL
int cache_clear_url(const char *url);

// Cleanup cache
void cache_cleanup(void);

#endif // CACHE_H 