#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <time.h>
#include <pthread.h>
#include <hiredis/hiredis.h>

// Structure to hold rate limit information for a domain
typedef struct {
    char *domain;
    double min_delay;        // Minimum delay between requests (seconds)
    double current_delay;    // Current delay between requests (seconds)
    time_t last_request;     // Timestamp of last request
    int consecutive_errors;  // Number of consecutive errors
    int max_errors;         // Maximum allowed consecutive errors
} domain_rate_t;

// Structure for the rate limiter
typedef struct {
    domain_rate_t *domains;  // Array of domain rate information
    int domain_count;        // Number of domains
    int domain_capacity;     // Capacity of domains array
    pthread_mutex_t mutex;   // Mutex for thread safety
    redisContext *redis_ctx; // Redis context for persistence
} rate_limiter_t;

// Create a new rate limiter
rate_limiter_t *rate_limiter_create(redisContext *redis_ctx);

// Destroy rate limiter
void rate_limiter_destroy(rate_limiter_t *limiter);

// Wait until it's safe to make a request to the domain
void rate_limiter_wait(const char *domain, rate_limiter_t *limiter);

// Update rate limits based on response
void rate_limiter_update(const char *domain, double response_time, int status_code, rate_limiter_t *limiter);

// Set crawl delay from robots.txt
void rate_limiter_set_crawl_delay(const char *domain, double delay, rate_limiter_t *limiter);

#endif // RATE_LIMITER_H 