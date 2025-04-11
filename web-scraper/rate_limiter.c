#include "rate_limiter.h"
#include "robots_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>  // For useconds_t

#define INITIAL_DOMAIN_CAPACITY 16
#define MAX_DELAY 60.0  // Maximum delay in seconds
#define MIN_DELAY 1.0   // Minimum delay in seconds
#define ERROR_PENALTY 2.0 // Multiplier for delay on errors
#define MAX_CONSECUTIVE_ERRORS 3

// Helper function to find or create domain rate entry
static domain_rate_t *get_domain_rate(const char *domain, rate_limiter_t *limiter) {
    pthread_mutex_lock(&limiter->mutex);
    
    // Search for existing domain
    for (int i = 0; i < limiter->domain_count; i++) {
        if (strcmp(limiter->domains[i].domain, domain) == 0) {
            pthread_mutex_unlock(&limiter->mutex);
            return &limiter->domains[i];
        }
    }
    
    // Resize array if needed
    if (limiter->domain_count >= limiter->domain_capacity) {
        int new_capacity = limiter->domain_capacity * 2;
        domain_rate_t *new_domains = realloc(limiter->domains, 
                                           new_capacity * sizeof(domain_rate_t));
        if (!new_domains) {
            pthread_mutex_unlock(&limiter->mutex);
            return NULL;
        }
        limiter->domains = new_domains;
        limiter->domain_capacity = new_capacity;
    }
    
    // Create new domain entry
    domain_rate_t *new_domain = &limiter->domains[limiter->domain_count++];
    new_domain->domain = strdup(domain);
    new_domain->min_delay = MIN_DELAY;
    new_domain->current_delay = MIN_DELAY;
    new_domain->last_request = 0;
    new_domain->consecutive_errors = 0;
    new_domain->max_errors = MAX_CONSECUTIVE_ERRORS;
    
    pthread_mutex_unlock(&limiter->mutex);
    return new_domain;
}

// Create a new rate limiter
rate_limiter_t *rate_limiter_create(redisContext *redis_ctx) {
    rate_limiter_t *limiter = malloc(sizeof(rate_limiter_t));
    if (!limiter) return NULL;
    
    limiter->domains = malloc(INITIAL_DOMAIN_CAPACITY * sizeof(domain_rate_t));
    if (!limiter->domains) {
        free(limiter);
        return NULL;
    }
    
    limiter->domain_count = 0;
    limiter->domain_capacity = INITIAL_DOMAIN_CAPACITY;
    limiter->redis_ctx = redis_ctx;
    pthread_mutex_init(&limiter->mutex, NULL);
    
    return limiter;
}

// Destroy rate limiter
void rate_limiter_destroy(rate_limiter_t *limiter) {
    if (!limiter) return;
    
    for (int i = 0; i < limiter->domain_count; i++) {
        free(limiter->domains[i].domain);
    }
    free(limiter->domains);
    pthread_mutex_destroy(&limiter->mutex);
    free(limiter);
}

// Wait until it's safe to make a request to the domain
void rate_limiter_wait(const char *domain, rate_limiter_t *limiter) {
    domain_rate_t *rate = get_domain_rate(domain, limiter);
    if (!rate) return;
    
    pthread_mutex_lock(&limiter->mutex);
    
    time_t now = time(NULL);
    double time_since_last = difftime(now, rate->last_request);
    
    if (time_since_last < rate->current_delay) {
        double sleep_time = rate->current_delay - time_since_last;
        pthread_mutex_unlock(&limiter->mutex);
        usleep((unsigned int)(sleep_time * 1000000));
        pthread_mutex_lock(&limiter->mutex);
    }
    
    rate->last_request = time(NULL);
    pthread_mutex_unlock(&limiter->mutex);
}

// Update rate limits based on response
void rate_limiter_update(const char *domain, double response_time, int status_code, rate_limiter_t *limiter) {
    domain_rate_t *rate = get_domain_rate(domain, limiter);
    if (!rate) return;
    
    pthread_mutex_lock(&limiter->mutex);
    
    if (status_code >= 400) {
        // Error response - increase delay
        rate->consecutive_errors++;
        if (rate->consecutive_errors >= rate->max_errors) {
            rate->current_delay = fmin(rate->current_delay * ERROR_PENALTY, MAX_DELAY);
            rate->consecutive_errors = 0;
        }
    } else {
        // Successful response - reset error count
        rate->consecutive_errors = 0;
        
        // Adjust delay based on response time
        if (response_time > rate->current_delay) {
            // Server is slow - increase delay
            rate->current_delay = fmin(rate->current_delay * 1.5, MAX_DELAY);
        } else if (response_time < rate->current_delay / 2) {
            // Server is fast - decrease delay
            rate->current_delay = fmax(rate->current_delay * 0.8, rate->min_delay);
        }
    }
    
    pthread_mutex_unlock(&limiter->mutex);
}

// Set crawl delay from robots.txt
void rate_limiter_set_crawl_delay(const char *domain, double delay, rate_limiter_t *limiter) {
    domain_rate_t *rate = get_domain_rate(domain, limiter);
    if (!rate) return;
    
    pthread_mutex_lock(&limiter->mutex);
    rate->min_delay = fmax(delay, MIN_DELAY);
    rate->current_delay = fmax(rate->current_delay, rate->min_delay);
    pthread_mutex_unlock(&limiter->mutex);
} 