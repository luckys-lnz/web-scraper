#include "scraper.h"
#include "cache.h"
#include "logger.h"
#include "rate_limiter.h"
#include "redis_helper.h"
#include "robots_parser.h"
#include "stats.h"
#include "url_processor.h"
#include "content_analyzer.h"
#include "fetch_url.h"
#include <pthread.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <hiredis/hiredis.h>

#define NUM_THREADS 8
#define QUEUE_SIZE 1000
#define STATS_INTERVAL 60  // Print stats every 60 seconds
#define MAX_MEMORY_MB 1024 // Maximum memory usage in MB
#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define MAX_URL_LENGTH 2048
#define MAX_RESPONSE_SIZE 1048576  // 1MB

// Global variables
extern pthread_mutex_t redis_mutex;  // Defined in redis_helper.c
extern rate_limiter_t *rate_limiter; // Defined in url_processor.c
extern pthread_mutex_t stats_mutex;  // Defined in stats.c
extern ScraperStats scraper_stats;   // Defined in stats.c
extern RedisStats redis_stats;       // Defined in stats.c
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
thread_pool_t *scraper_pool = NULL;

// Global variables for CURL and Redis
static CURL *curl = NULL;
static redisContext *redis = NULL;

// Global scraper configuration
static scraper_config_t scraper_config = {
    .max_depth = 3,
    .max_pages = 1000,
    .max_memory_mb = MAX_MEMORY_MB,
    .respect_robots = 1,
    .use_javascript = 0,
    .extract_media = 1,
    .analyze_content = 1,
    .track_trends = 1,
    .force_rescrape = 0,
    .user_agent = "AI-Powered Web Scraper/1.0",
    .request_timeout = 30,
    .retry_count = 3,
    .retry_delay = 5
};

// Function to extract base URL from full URL
void split_url(const char *url, char *base_url, char *target_path) {
  const char *slash = strchr(url + 8, '/');
  if (slash) {
    size_t base_len = slash - url;
    strncpy(base_url, url, base_len);
    base_url[base_len] = '\0';
    strcpy(target_path, slash);
  } else {
    strcpy(base_url, url);
    target_path[0] = '/';
    target_path[1] = '\0';
  }
}

// Initialize thread pool with specified number of threads
void init_scraper_pool(int thread_count) {
  LOG_INFO("Creating thread pool with %d threads and queue size %d",
           thread_count, QUEUE_SIZE);
  scraper_pool = thread_pool_create(thread_count, QUEUE_SIZE);
  if (!scraper_pool) {
    LOG_ERROR("Failed to create thread pool");
    exit(EXIT_FAILURE);
  }
  LOG_INFO("Thread pool created successfully");
}

// Cleanup thread pool
void cleanup_scraper_pool() {
  if (scraper_pool) {
    thread_pool_destroy(scraper_pool);
    scraper_pool = NULL;
  }
}

// Monitor thread function
void *monitor_thread(void *arg) {
    int stats_interval = *(int *)arg;
    free(arg);  // Free the allocated memory
    
    while (1) {
        sleep(stats_interval);
        print_stats();

        // Check memory usage
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        if (usage.ru_maxrss > MAX_MEMORY_MB * 1024) {
            fprintf(stderr, "Warning: Memory usage exceeded %d MB\n", MAX_MEMORY_MB);
        }
    }
    return NULL;
}

int init_scraper() {
    // Initialize logger
    logger_init("crawler.log");
    
    // Initialize CURL
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return -1;
    }

    // Initialize Redis connection
    if (!init_redis(REDIS_HOST, REDIS_PORT)) {
        fprintf(stderr, "Failed to initialize Redis\n");
        curl_easy_cleanup(curl);
        return -1;
    }
    redis = get_redis_context();

    // Initialize thread pool
    init_scraper_pool(NUM_THREADS);
    if (!scraper_pool) {
        fprintf(stderr, "Failed to initialize thread pool\n");
        cleanup_scraper();
        return -1;
    }

    // Initialize URL processor
    if (init_url_processor(redis) != 0) {
        fprintf(stderr, "Failed to initialize URL processor\n");
        cleanup_scraper();
        return -1;
    }

    // Initialize stats
    init_stats();

    // Start monitor thread
    pthread_t monitor_tid;
    int *stats_interval = malloc(sizeof(int));
    if (!stats_interval) {
        fprintf(stderr, "Failed to allocate memory for stats interval\n");
        cleanup_scraper();
        return -1;
    }
    *stats_interval = STATS_INTERVAL;
    if (pthread_create(&monitor_tid, NULL, monitor_thread, stats_interval) != 0) {
        fprintf(stderr, "Failed to create monitor thread\n");
        free(stats_interval);
        cleanup_scraper();
        return -1;
    }
    pthread_detach(monitor_tid);

    LOG_INFO("Scraper initialized successfully");
    return 0;
}

int process_url(const char *url) {
    if (!url || !scraper_pool) {
        LOG_ERROR("Invalid URL or scraper not initialized");
        return -1;
    }

    // Check if thread pool queue is full
    if (thread_pool_get_queue_size(scraper_pool) >= QUEUE_SIZE) {
        LOG_ERROR("Thread pool queue is full");
        return -1;
    }

    // Create a task for the URL
    url_task_t *task = malloc(sizeof(url_task_t));
    if (!task) {
        LOG_ERROR("Failed to allocate memory for URL task");
        return -1;
    }

    // Copy URL to task
    task->url = strdup(url);
    if (!task->url) {
        LOG_ERROR("Failed to allocate memory for URL string");
        free(task);
        return -1;
    }
    
    // Set task properties
    task->priority = 1;
    task->depth = 0;
    task->parent_url = NULL;

    // Add task to thread pool
    if (!thread_pool_add_task(scraper_pool, process_url_thread, task)) {
        LOG_ERROR("Failed to add URL task to thread pool");
        free(task->url);
        free(task);
        return -1;
    }

    LOG_INFO("Added URL to processing queue: %s", url);
    return 0;
}

void cleanup_scraper() {
    LOG_INFO("Cleaning up scraper resources");
    
    // Cleanup thread pool
    cleanup_scraper_pool();
    
    // Cleanup URL processor
    cleanup_url_processor();
    
    // Cleanup CURL
    if (curl) {
        curl_easy_cleanup(curl);
        curl = NULL;
    }
    
    // Cleanup Redis
    if (redis) {
        redisFree(redis);
        redis = NULL;
    }
    
    // Cleanup logger
    logger_close();
    
    LOG_INFO("Scraper cleanup completed");
}

// AI-powered content analysis functions
content_analysis_t *analyze_url_content(const char *url) {
    if (!url || !redis) {
        LOG_ERROR("Invalid URL or Redis not initialized");
        return NULL;
    }
    
    // Check if analysis already exists in Redis
    content_analysis_t *analysis = get_analysis_results(redis, url);
    if (analysis) {
        LOG_INFO("Retrieved existing analysis for URL: %s", url);
        return analysis;
    }
    
    // Fetch URL content
    LOG_INFO("Fetching content from URL for analysis: %s", url);
    struct Memory chunk = {0};
    fetch_url(url, &chunk);
    if (!chunk.response) {
        LOG_ERROR("Failed to fetch URL for analysis: %s", url);
        return NULL;
    }
    
    // Analyze content
    LOG_INFO("Analyzing content from URL: %s", url);
    analysis = analyze_content(chunk.response, url);
    
    // Free chunk
    free(chunk.response);
    
    if (analysis) {
        LOG_INFO("Content analysis completed for URL: %s", url);
        
        // Store analysis results
        if (store_analysis_results(redis, url, analysis) == 0) {
            LOG_INFO("Stored analysis results for URL: %s", url);
        } else {
            LOG_WARNING("Failed to store analysis results for URL: %s", url);
        }
    } else {
        LOG_WARNING("Failed to analyze content for URL: %s", url);
    }
    
    return analysis;
}

trend_data_t **get_trending_topics(int limit) {
    if (!redis || limit <= 0) {
        LOG_ERROR("Invalid parameters for getting trending topics");
        return NULL;
    }
    
    LOG_INFO("Getting trending topics (limit: %d)", limit);
    trend_data_t **trends = detect_trends(redis, limit);
    
    if (trends) {
        LOG_INFO("Retrieved %d trending topics", limit);
    } else {
        LOG_WARNING("Failed to retrieve trending topics");
    }
    
    return trends;
}

// Configuration functions
void set_scraper_config(scraper_config_t *config) {
    if (!config) {
        LOG_ERROR("Invalid scraper configuration");
        return;
    }
    
    // Copy configuration
    memcpy(&scraper_config, config, sizeof(scraper_config_t));
    
    // Make a deep copy of user agent string
    if (config->user_agent) {
        free(scraper_config.user_agent);
        scraper_config.user_agent = strdup(config->user_agent);
    }
    
    LOG_INFO("Scraper configuration updated");
}

scraper_config_t *get_scraper_config(void) {
    // Create a copy of the configuration
    scraper_config_t *config = malloc(sizeof(scraper_config_t));
    if (!config) {
        LOG_ERROR("Failed to allocate memory for scraper configuration");
        return NULL;
    }
    
    // Copy configuration
    memcpy(config, &scraper_config, sizeof(scraper_config_t));
    
    // Make a deep copy of user agent string
    if (scraper_config.user_agent) {
        config->user_agent = strdup(scraper_config.user_agent);
    }
    
    return config;
}
