#include "scraper.h"
#include "cache.h"
#include "logger.h"
#include "rate_limiter.h"
#include "redis_helper.h"
#include "robots_parser.h"
#include "stats.h"
#include "url_processor.h"
#include <pthread.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#define NUM_THREADS 8
#define QUEUE_SIZE 1000
#define STATS_INTERVAL 60  // Print stats every 60 seconds
#define MAX_MEMORY_MB 1024 // Maximum memory usage in MB
#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379

// Global variables
extern pthread_mutex_t redis_mutex;  // Defined in redis_helper.c
extern rate_limiter_t *rate_limiter; // Defined in url_processor.c
extern pthread_mutex_t stats_mutex;  // Defined in stats.c
extern ScraperStats scraper_stats;   // Defined in stats.c
extern RedisStats redis_stats;       // Defined in stats.c
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
thread_pool_t *scraper_pool = NULL;

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

int main(int argc, char *argv[]) {
  // Default configuration
  int num_threads = NUM_THREADS;
  int max_memory_mb = MAX_MEMORY_MB;
  int stats_interval = STATS_INTERVAL;
  const char *log_file = "crawler.log";
  const char *redis_host = REDIS_HOST;
  int redis_port = REDIS_PORT;

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      num_threads = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--memory") == 0 && i + 1 < argc) {
      max_memory_mb = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--stats-interval") == 0 && i + 1 < argc) {
      stats_interval = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--log-file") == 0 && i + 1 < argc) {
      log_file = argv[++i];
    } else if (strcmp(argv[i], "--redis-host") == 0 && i + 1 < argc) {
      redis_host = argv[++i];
    } else if (strcmp(argv[i], "--redis-port") == 0 && i + 1 < argc) {
      redis_port = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--help") == 0) {
      printf("Usage: %s [options]\n", argv[0]);
      printf("Options:\n");
      printf("  --threads N        Number of worker threads (default: %d)\n",
             NUM_THREADS);
      printf("  --memory N         Maximum memory usage in MB (default: %d)\n",
             MAX_MEMORY_MB);
      printf("  --stats-interval N Statistics reporting interval in seconds "
             "(default: %d)\n",
             STATS_INTERVAL);
      printf("  --log-file FILE    Log file path (default: crawler.log)\n");
      printf("  --redis-host HOST  Redis host (default: %s)\n", REDIS_HOST);
      printf("  --redis-port PORT  Redis port (default: %d)\n", REDIS_PORT);
      printf("  --help             Show this help message\n");
      return EXIT_SUCCESS;
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      fprintf(stderr, "Use --help for usage information\n");
      return EXIT_FAILURE;
    }
  }

  // Initialize logger first
  logger_init(log_file);
  LOG_INFO("Starting web crawler with configuration:");
  LOG_INFO("  Threads: %d", num_threads);
  LOG_INFO("  Memory limit: %d MB", max_memory_mb);
  LOG_INFO("  Stats interval: %d seconds", stats_interval);
  LOG_INFO("  Redis host: %s", redis_host);
  LOG_INFO("  Redis port: %d", redis_port);

  // Initialize CURL
  LOG_INFO("Initializing CURL");
  if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
    LOG_ERROR("Failed to initialize CURL");
    return EXIT_FAILURE;
  }

  // Initialize Redis
  LOG_INFO("Initializing Redis connection to %s:%d", redis_host, redis_port);
  if (init_redis(redis_host, redis_port) == 0) {
    LOG_ERROR("Failed to initialize Redis. Please check the error messages "
              "above for instructions.");
    curl_global_cleanup();
    return EXIT_FAILURE;
  }

  // Initialize cache (depends on Redis)
  LOG_INFO("Initializing cache");
  if (cache_init() != 0) {
    LOG_ERROR("Failed to initialize cache");
    close_redis();
    curl_global_cleanup();
    return EXIT_FAILURE;
  }

  // Initialize URL processor (includes rate limiter and thread pool)
  LOG_INFO("Initializing URL processor");
  if (!init_url_processor(redis_ctx, num_threads)) {
    LOG_ERROR("Failed to initialize URL processor");
    cache_cleanup();
    close_redis();
    curl_global_cleanup();
    return EXIT_FAILURE;
  }

  // Initialize stats
  LOG_INFO("Initializing statistics");
  init_stats();

  // Create monitor thread
  pthread_t monitor_tid;
  if (pthread_create(&monitor_tid, NULL, monitor_thread,
                     (void *)&stats_interval) != 0) {
    LOG_ERROR("Error creating monitor thread");
    cleanup_url_processor();
    cache_cleanup();
    close_redis();
    curl_global_cleanup();
    return EXIT_FAILURE;
  }

  // Add seed URLs
  const char *seed_urls[] = {"https://example.com", "https://www.wikipedia.org",
                             "https://www.github.com"};
  int num_seeds = sizeof(seed_urls) / sizeof(seed_urls[0]);

  LOG_INFO("Adding %d seed URLs to queue", num_seeds);
  for (int i = 0; i < num_seeds; i++) {
    LOG_INFO("Adding seed URL: %s", seed_urls[i]);
    push_url_to_queue(seed_urls[i], 0);
  }

  // Main processing loop
  LOG_INFO("Starting main processing loop");
  while (1) {
    // Check Redis connection before processing
    if (!redis_ctx || redis_ctx->err) {
      LOG_ERROR("Redis connection lost, attempting to reconnect...");
      close_redis();
      if (init_redis(redis_host, redis_port) != 0) {
        LOG_ERROR("Failed to reconnect to Redis, waiting before retry...");
        sleep(5);
        continue;
      }
      LOG_INFO("Redis reconnected successfully");

      // Reinitialize cache after Redis reconnection
      if (cache_init() != 0) {
        LOG_ERROR("Failed to reinitialize cache after Redis reconnection");
        close_redis();
        sleep(5);
        continue;
      }
      LOG_INFO("Cache reinitialized successfully");
    }

    // Fetch URL from queue
    char *url = fetch_url_from_queue();
    if (!url) {
      LOG_INFO("No more URLs in queue, waiting...");
      sleep(1);
      continue;
    }

    // Create task for URL
    url_task_t *task = malloc(sizeof(url_task_t));
    if (!task) {
      LOG_ERROR("Failed to allocate memory for task");
      free(url);
      continue;
    }

    task->url = url;
    task->priority = 0;

    // Add task to thread pool
    if (!thread_pool_add_task(scraper_pool, process_url, task)) {
      LOG_ERROR("Failed to add URL to thread pool: %s", url);
      free(task->url);
      free(task);
      continue;
    }

    LOG_INFO("Added URL to thread pool: %s", url);
  }

  // Cleanup
  LOG_INFO("Shutting down crawler");
  cleanup_url_processor();
  cache_cleanup();
  close_redis();
  curl_global_cleanup();
  logger_close();
  return EXIT_SUCCESS;
}
