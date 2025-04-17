#ifndef URL_PROCESSOR_H
#define URL_PROCESSOR_H

#include "scraper.h"
#include "rate_limiter.h"
#include <hiredis/hiredis.h>

// Function prototypes
void *process_url(void *arg);
void split_url(const char *url, char *base_url, char *target_path);

// Initialize URL processor components
int init_url_processor(redisContext *redis_ctx, int num_threads);

// Cleanup URL processor components
void cleanup_url_processor(void);

#endif // URL_PROCESSOR_H 