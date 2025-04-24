#include "url_processor.h"
#include "redis_helper.h"
#include "robots_parser.h"
#include "cache.h"
#include "logger.h"
#include "stats.h"
#include "thread_pool.h"
#include "fetch_url.h"
#include "scraper.h"
#include "rate_limiter.h"
#include "content_analyzer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <time.h>

// Global variables
extern thread_pool_t *scraper_pool;  // Defined in scraper.c
rate_limiter_t *rate_limiter = NULL; // Global rate limiter instance

// Process a single URL
void *process_url_thread(void *arg) {
    url_task_t *task = (url_task_t *)arg;
    if (!task || !task->url) {
        LOG_ERROR("Invalid task or URL");
        return NULL;
    }

    redisContext *ctx = get_redis_context();
    if (!ctx) {
        LOG_ERROR("Failed to get Redis context");
        free(task->url);
        free(task);
        return NULL;
    }

    LOG_INFO("Starting to process URL: %s", task->url);

    // Check if URL has been visited
    if (is_visited(task->url)) {
        // Get scraper configuration to check force_rescrape flag
        scraper_config_t *config = get_scraper_config();
        if (config && config->force_rescrape) {
            LOG_INFO("Force re-scraping enabled, processing URL despite being visited: %s", task->url);
            printf("\n\033[1;33m⚠️  INFO: URL '%s' has already been visited, but force re-scraping is enabled.\033[0m\n\n");
            free(config->user_agent);
            free(config);
        } else {
            // Get analysis data if available
            redisReply *reply = redisCommand(ctx, "HGETALL analysis:%s", task->url);
            if (reply && reply->type == REDIS_REPLY_ARRAY) {
                printf("\n\033[1;33m⚠️  ALERT: URL '%s' has already been visited!\033[0m\n", task->url);
                printf("\033[1;36mPrevious Analysis Data:\033[0m\n");
                for (size_t i = 0; i < reply->elements; i += 2) {
                    if (i + 1 < reply->elements) {
                        printf("  %s: %s\n", reply->element[i]->str, reply->element[i+1]->str);
                    }
                }
                freeReplyObject(reply);
            } else {
                printf("\n\033[1;33m⚠️  ALERT: URL '%s' has already been visited!\033[0m\n", task->url);
                if (reply) freeReplyObject(reply);
            }
            
            // Get cache data if available
            reply = redisCommand(ctx, "HGET cache:%s type", task->url);
            if (reply && reply->type == REDIS_REPLY_STRING) {
                printf("\033[1;36mCache Type:\033[0m %s\n", reply->str);
                freeReplyObject(reply);
            }
            
            printf("\033[1;32m✓ URL processing skipped\033[0m\n\n");
            LOG_INFO("URL already visited: %s", task->url);
            free(task->url);
            free(task);
            return NULL;
        }
    }

    // Extract domain for rate limiting
    char *domain = extract_domain(task->url);
    if (!domain) {
        LOG_ERROR("Failed to extract domain from URL: %s", task->url);
        free(task->url);
        free(task);
        return NULL;
    }
    LOG_INFO("Extracted domain: %s", domain);

    // Wait for rate limit
    LOG_INFO("Waiting for rate limit on domain: %s", domain);
    rate_limiter_wait(domain, rate_limiter);
    LOG_INFO("Rate limit wait complete for domain: %s", domain);

    // Split URL into base and path for robots.txt check
    char base_url[256], target_path[1024];
    split_url(task->url, base_url, target_path);
    LOG_INFO("Split URL - base: %s, path: %s", base_url, target_path);

    // Fetch robots.txt for the domain
    LOG_INFO("Fetching robots.txt for domain: %s", domain);
    fetch_robots_txt(base_url, rate_limiter);

    // Check robots.txt
    if (!is_crawl_allowed(base_url, target_path, rate_limiter)) {
        LOG_INFO("URL not allowed by robots.txt: %s", task->url);
        free(domain);
        free(task->url);
        free(task);
        return NULL;
    }
    LOG_INFO("URL allowed by robots.txt: %s", task->url);

    // Fetch URL content
    LOG_INFO("Fetching content from URL: %s", task->url);
    struct Memory chunk = {0};
    fetch_url(task->url, &chunk);
    if (!chunk.response) {
        LOG_ERROR("Failed to fetch URL: %s", task->url);
        free(domain);
        free(task->url);
        free(task);
        return NULL;
    }
    LOG_INFO("Successfully fetched content from URL: %s (size: %zu bytes)", task->url, chunk.size);

    // Store in cache
    LOG_INFO("Storing content in cache for URL: %s", task->url);
    if (cache_store_content(ctx, task->url, chunk.response, chunk.size, "text/html", 200) != 0) {
        LOG_WARNING("Failed to cache content for URL: %s", task->url);
    } else {
        LOG_INFO("Successfully cached content for URL: %s", task->url);
    }

    // Analyze content using AI
    LOG_INFO("Analyzing content from URL: %s", task->url);
    content_analysis_t *analysis = analyze_content(chunk.response, task->url);
    if (analysis) {
        LOG_INFO("Content analysis completed for URL: %s", task->url);
        
        // Store analysis results
        if (store_analysis_results(ctx, task->url, analysis) == 0) {
            LOG_INFO("Stored analysis results for URL: %s", task->url);
        } else {
            LOG_WARNING("Failed to store analysis results for URL: %s", task->url);
        }
        
        // Free analysis results
        free_content_analysis(analysis);
    } else {
        LOG_WARNING("Failed to analyze content for URL: %s", task->url);
    }

    // Extract and process content
    LOG_INFO("Extracting content from URL: %s", task->url);
    extract_title(chunk.response);
    extract_meta(chunk.response);
    extract_hrefs(chunk.response, task->url);

    // Mark URL as visited
    LOG_INFO("Marking URL as visited: %s", task->url);
    const char *urls[] = {task->url};
    mark_visited_bulk(urls, 1);

    // Update statistics
    LOG_INFO("Updating statistics for URL: %s", task->url);
    update_stats(chunk.size, 0, 0);

    // Cleanup
    LOG_INFO("Cleaning up resources for URL: %s", task->url);
    free(chunk.response);
    free(domain);
    LOG_INFO("Finished processing URL: %s", task->url);
    free(task->url);
    free(task);
    return NULL;
}

// Initialize URL processor components
int init_url_processor(redisContext *ctx) {
    // Initialize rate limiter
    rate_limiter = rate_limiter_create(ctx);
    if (!rate_limiter) {
        LOG_ERROR("Failed to create rate limiter");
        return -1;
    }

    // Initialize cache
    if (!cache_init(ctx)) {
        LOG_ERROR("Failed to initialize cache");
        rate_limiter_destroy(rate_limiter);
        return -1;
    }
    
    // Initialize content analyzer
    if (init_content_analyzer(ctx) != 0) {
        LOG_ERROR("Failed to initialize content analyzer");
        rate_limiter_destroy(rate_limiter);
        return -1;
    }

    // Thread pool is already initialized in init_scraper()
    if (!scraper_pool) {
        LOG_ERROR("Thread pool not initialized");
        rate_limiter_destroy(rate_limiter);
        return -1;
    }

    return 0;  // Success
}

// Cleanup URL processor components
void cleanup_url_processor(void) {
    if (rate_limiter) {
        rate_limiter_destroy(rate_limiter);
        rate_limiter = NULL;
    }
    
    // Cleanup content analyzer
    cleanup_content_analyzer();
} 