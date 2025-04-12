#ifndef STATS_H
#define STATS_H

#include <time.h>
#include <pthread.h>

// Performance monitoring structures
typedef struct {
    unsigned long urls_processed;
    unsigned long urls_skipped;
    unsigned long urls_disallowed;
    unsigned long bytes_downloaded;
    time_t start_time;
    time_t last_report_time;
} ScraperStats;

typedef struct {
    unsigned long redis_ops;
    unsigned long redis_errors;
    unsigned long redis_latency_ms;
} RedisStats;

// Global stats
extern ScraperStats scraper_stats;
extern RedisStats redis_stats;
extern pthread_mutex_t stats_mutex;

// Function prototypes
void init_stats(void);
void update_stats(unsigned long bytes, int skipped, int disallowed);
void update_redis_stats(int ops, int errors, int latency_ms);
void print_stats(void);

#endif // STATS_H 