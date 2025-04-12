#include "stats.h"
#include "redis_helper.h"
#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <sys/resource.h>

// Global variables
ScraperStats scraper_stats = {0};
RedisStats redis_stats = {0};
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize performance monitoring
void init_stats(void) {
    scraper_stats.start_time = time(NULL);
    scraper_stats.last_report_time = scraper_stats.start_time;
    scraper_stats.urls_processed = 0;
    scraper_stats.urls_skipped = 0;
    scraper_stats.urls_disallowed = 0;
    scraper_stats.bytes_downloaded = 0;
    
    redis_stats.redis_ops = 0;
    redis_stats.redis_errors = 0;
    redis_stats.redis_latency_ms = 0;
}

// Update scraper statistics
void update_stats(unsigned long bytes, int skipped, int disallowed) {
    pthread_mutex_lock(&stats_mutex);
    scraper_stats.bytes_downloaded += bytes;
    if (skipped) scraper_stats.urls_skipped++;
    if (disallowed) scraper_stats.urls_disallowed++;
    scraper_stats.urls_processed++;
    
    // Update Redis stats
    redis_stats.redis_ops += 3; // SISMEMBER, RPUSH processed_urls, SADD visited_urls
    pthread_mutex_unlock(&stats_mutex);
}

// Update Redis statistics
void update_redis_stats(int ops, int errors, int latency_ms) {
    pthread_mutex_lock(&stats_mutex);
    redis_stats.redis_ops += ops;
    redis_stats.redis_errors += errors;
    redis_stats.redis_latency_ms += latency_ms;
    pthread_mutex_unlock(&stats_mutex);
}

// Print current statistics
void print_stats(void) {
    pthread_mutex_lock(&stats_mutex);
    time_t now = time(NULL);
    double elapsed = difftime(now, scraper_stats.start_time);
    
    printf("\n=== Performance Statistics ===\n");
    printf("Elapsed time: %.2f seconds\n", elapsed);
    
    if (elapsed > 0) {
        printf("URLs processed: %lu (%.2f URLs/sec)\n", 
               scraper_stats.urls_processed,
               scraper_stats.urls_processed / elapsed);
        printf("URLs skipped: %lu\n", scraper_stats.urls_skipped);
        printf("URLs disallowed: %lu\n", scraper_stats.urls_disallowed);
        printf("Bytes downloaded: %lu (%.2f MB)\n", 
               scraper_stats.bytes_downloaded,
               scraper_stats.bytes_downloaded / (1024.0 * 1024.0));
        printf("Redis operations: %lu (%.2f ops/sec)\n",
               redis_stats.redis_ops,
               redis_stats.redis_ops / elapsed);
        printf("Redis errors: %lu\n", redis_stats.redis_errors);
        
        if (redis_stats.redis_ops > 0) {
            printf("Average Redis latency: %.2f ms\n",
                   redis_stats.redis_latency_ms / (double)redis_stats.redis_ops);
        } else {
            printf("Average Redis latency: N/A (no operations performed)\n");
        }
    } else {
        printf("URLs processed: %lu (N/A URLs/sec)\n", scraper_stats.urls_processed);
        printf("URLs skipped: %lu\n", scraper_stats.urls_skipped);
        printf("URLs disallowed: %lu\n", scraper_stats.urls_disallowed);
        printf("Bytes downloaded: %lu (0.00 MB)\n", scraper_stats.bytes_downloaded);
        printf("Redis operations: %lu (N/A ops/sec)\n", redis_stats.redis_ops);
        printf("Redis errors: %lu\n", redis_stats.redis_errors);
        printf("Average Redis latency: N/A (no operations performed)\n");
    }
    
    // Get memory usage
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("Memory usage: %.2f MB\n", 
           usage.ru_maxrss / 1024.0);
    
    // Print processed URLs - only if Redis is available
    if (is_redis_initialized()) {
        pthread_mutex_lock(&redis_mutex);
        redisReply *reply = redisCommand(redis_ctx, "LRANGE processed_urls 0 -1");
        pthread_mutex_unlock(&redis_mutex);
        
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            printf("\n=== Processed URLs ===\n");
            for (size_t i = 0; i < reply->elements; i++) {
                printf("%zu. %s\n", i + 1, reply->element[i]->str);
            }
        } else {
            printf("\n=== Processed URLs ===\n");
            printf("No URLs processed yet or Redis not available\n");
        }
        if (reply) freeReplyObject(reply);
    } else {
        printf("\n=== Processed URLs ===\n");
        printf("Redis not available - cannot display processed URLs\n");
    }
    
    scraper_stats.last_report_time = now;
    pthread_mutex_unlock(&stats_mutex);
} 