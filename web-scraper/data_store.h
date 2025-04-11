#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <libpq-fe.h>
#include <stdbool.h>

// Data structures for extracted content
typedef struct {
    char *url;
    char *title;
    char *description;
    char *keywords;
    char *author;
    time_t crawl_time;
    size_t content_size;
    char *content_type;
    int status_code;
    double response_time;
} page_metadata_t;

typedef struct {
    char *url;
    char *src;
    char *alt;
    int width;
    int height;
} image_data_t;

// Initialize database connection
int data_store_init(const char *conninfo);

// Close database connection
void data_store_cleanup();

// Store page metadata
bool store_page_metadata(const page_metadata_t *metadata);

// Store image data
bool store_image_data(const image_data_t *image);

// Store link relationship
bool store_link_relationship(const char *from_url, const char *to_url);

// Get page metadata by URL
bool get_page_metadata(const char *url, page_metadata_t *metadata);

// Get images for a page
bool get_page_images(const char *url, image_data_t **images, int *count);

// Get links from a page
bool get_page_links(const char *url, char ***links, int *count);

#endif // DATA_STORE_H 