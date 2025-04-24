#ifndef URL_PROCESSOR_H
#define URL_PROCESSOR_H

#include <hiredis/hiredis.h>
#include "types.h"

// Initialize URL processor
int init_url_processor(redisContext *ctx);

// Process a URL in a thread
void *process_url_thread(void *arg);

// Cleanup URL processor
void cleanup_url_processor(void);

#endif // URL_PROCESSOR_H 