#include "url_processor.h"
#include "redis_helper.h"
#include "robots_parser.h"
#include "cache.h"
#include "logger.h"
#include "stats.h"
#include "thread_pool.h"
#include "fetch_url.h"
#include "scraper.h"
#include <string.h>
#include <stdlib.h>

// Global variables
extern thread_pool_t *scraper_pool;  // Defined in scraper.c
static rate_limiter_t *rate_limiter = NULL;

// Process a single URL
void *process_url(void *arg) {
    if (!arg) {
        LOG_ERROR("process_url called with NULL argument");
        return NULL;
    }

    url_task_t *task = (url_task_t *)arg;
    if (!task->url) {
        LOG_ERROR("process_url called with NULL url");
        free(task);
        return NULL;
    }

    LOG_INFO("Processing URL: %s", task->url);

    // Check if URL has been visited
    if (is_visited(task->url)) {
        LOG_INFO("URL already visited: %s", task->url);
        free(task->url);
        free(task);
        return NULL;
    }

    // Extract domain for rate limiting
    char *domain = extract_domain(task->url);
    if (!domain) {
        LOG_ERROR("Failed to extract domain from URL: %s", task->url);
        free(task->url);
        free(task);
        return NULL;
    }

    // Wait for rate limit
    rate_limiter_wait(domain, rate_limiter);

    // Split URL into base and path for robots.txt check
    char base_url[256], target_path[1024];
    split_url(task->url, base_url, target_path);

    // Check robots.txt
    if (!is_crawl_allowed(base_url, target_path)) {
        LOG_INFO("URL not allowed by robots.txt: %s", task->url);
        free(domain);
        free(task->url);
        free(task);
        return NULL;
    }

    // Fetch URL content
    struct Memory chunk = {0};
    fetch_url(task->url, &chunk);
    if (!chunk.response) {
        LOG_ERROR("Failed to fetch URL: %s", task->url);
        free(domain);
        free(task->url);
        free(task);
        return NULL;
    }

    // Store in cache
    if (cache_store_content(task->url, chunk.response, chunk.size, "text/html", 200) != 0) {
        LOG_WARNING("Failed to cache content for URL: %s", task->url);
    }

    // Extract and process content
    extract_title(chunk.response);
    extract_meta(chunk.response);
    extract_hrefs(chunk.response, task->url);

    // Mark URL as visited
    const char *urls[] = {task->url};
    mark_visited_bulk(urls, 1);

    // Update statistics
    update_stats(chunk.size, 0, 0);

    // Cleanup
    free(chunk.response);
    free(domain);
    free(task->url);
    free(task);
    return NULL;
}

// Initialize URL processor components
int init_url_processor(redisContext *redis_ctx, int num_threads) {
    // Initialize rate limiter
    rate_limiter = rate_limiter_create(redis_ctx);
    if (!rate_limiter) {
        LOG_ERROR("Failed to create rate limiter");
        return 0;
    }

    // Initialize thread pool
    LOG_INFO("Initializing thread pool with %d threads", num_threads);
    scraper_pool = thread_pool_create(num_threads, 1000);
    if (!scraper_pool) {
        LOG_ERROR("Failed to create thread pool");
        rate_limiter_destroy(rate_limiter);
        return 0;
    }

    return 1;
}

// Cleanup URL processor components
void cleanup_url_processor(void) {
    if (scraper_pool) {
        thread_pool_destroy(scraper_pool);
        scraper_pool = NULL;
    }
    if (rate_limiter) {
        rate_limiter_destroy(rate_limiter);
        rate_limiter = NULL;
    }
} 