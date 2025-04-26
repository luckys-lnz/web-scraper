#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "scraper.h"
#include "logger.h"
#include "redis_helper.h"
#include "url_processor.h"
#include "thread_pool.h"
#include "rate_limiter.h"
#include "types.h"
#include "content_analyzer.h"

// External declarations
extern thread_pool_t *scraper_pool;  // Defined in scraper.c
extern rate_limiter_t *rate_limiter; // Defined in url_processor.c

// Function declarations
int init_scraper(void);
void cleanup_scraper(void);
int process_url(const char *url);
void *process_url_thread(void *arg);

// Print usage information
void print_usage(const char *program_name) {
    printf("Usage: %s [options] <url>\n", program_name);
    printf("Options:\n");
    printf("  -h, --help                 Display this help message\n");
    printf("  -a, --analyze <url>        Analyze content of a URL\n");
    printf("  -t, --trends [limit]       Show trending topics (default limit: 10)\n");
    printf("  -c, --config               Show current scraper configuration\n");
    printf("  -d, --depth <n>            Set maximum crawl depth (default: 3)\n");
    printf("  -p, --pages <n>            Set maximum pages to crawl (default: 1000)\n");
    printf("  -m, --memory <n>           Set maximum memory usage in MB (default: 1024)\n");
    printf("  -j, --javascript           Enable JavaScript rendering\n");
    printf("  -r, --no-robots            Disable robots.txt compliance\n");
    printf("  -f, --force                Force re-scraping of already visited URLs\n");
    printf("  -v, --verbose              Enable verbose output\n");
}

// Print content analysis results
void print_analysis(content_analysis_t *analysis) {
    if (!analysis) {
        printf("No analysis results available.\n");
        return;
    }
    
    printf("\n=== Content Analysis Results ===\n");
    
    if (analysis->title) {
        printf("Title: %s\n", analysis->title);
    }
    
    if (analysis->description) {
        printf("Description: %s\n", analysis->description);
    }
    
    if (analysis->keywords) {
        printf("Keywords: %s\n", analysis->keywords);
    }
    
    if (analysis->author) {
        printf("Author: %s\n", analysis->author);
    }
    
    if (analysis->publish_date) {
        printf("Publish Date: %s\n", analysis->publish_date);
    }
    
    if (analysis->language) {
        printf("Language: %s\n", analysis->language);
    }
    
    printf("Sentiment Score: %.2f\n", analysis->sentiment_score);
    
    if (analysis->topic_count > 0 && analysis->topics) {
        printf("Topics: ");
        for (int i = 0; i < analysis->topic_count; i++) {
            printf("%s%s", analysis->topics[i], i < analysis->topic_count - 1 ? ", " : "");
        }
        printf("\n");
    }
    
    if (analysis->entity_count > 0 && analysis->entities) {
        printf("Entities: ");
        for (int i = 0; i < analysis->entity_count; i++) {
            printf("%s%s", analysis->entities[i], i < analysis->entity_count - 1 ? ", " : "");
        }
        printf("\n");
    }
    
    if (analysis->category_count > 0 && analysis->categories) {
        printf("Categories: ");
        for (int i = 0; i < analysis->category_count; i++) {
            printf("%s%s", analysis->categories[i], i < analysis->category_count - 1 ? ", " : "");
        }
        printf("\n");
    }
    
    printf("==============================\n\n");
}

// Print trend data
void print_trends(trend_data_t **trends, int limit) {
    if (!trends || !trends[0]) {
        printf("No trending topics available.\n");
        return;
    }
    
    printf("\n=== Trending Topics ===\n");
    
    for (int i = 0; i < limit && trends[i]; i++) {
        printf("%d. %s (Frequency: %d, Growth: %.2f%%)\n", 
               i + 1, trends[i]->topic, trends[i]->frequency, trends[i]->growth_rate);
        
        if (trends[i]->related_topic_count > 0 && trends[i]->related_topics) {
            printf("   Related: ");
            for (int j = 0; j < trends[i]->related_topic_count; j++) {
                printf("%s%s", trends[i]->related_topics[j], 
                       j < trends[i]->related_topic_count - 1 ? ", " : "");
            }
            printf("\n");
        }
    }
    
    printf("=====================\n\n");
}

// Print scraper configuration
void print_config(scraper_config_t *config) {
    if (!config) {
        printf("No configuration available.\n");
        return;
    }
    
    printf("\n=== Scraper Configuration ===\n");
    printf("Max Depth: %d\n", config->max_depth);
    printf("Max Pages: %d\n", config->max_pages);
    printf("Max Memory: %d MB\n", config->max_memory_mb);
    printf("Respect Robots: %s\n", config->respect_robots ? "Yes" : "No");
    printf("Use JavaScript: %s\n", config->use_javascript ? "Yes" : "No");
    printf("Extract Media: %s\n", config->extract_media ? "Yes" : "No");
    printf("Analyze Content: %s\n", config->analyze_content ? "Yes" : "No");
    printf("Track Trends: %s\n", config->track_trends ? "Yes" : "No");
    printf("Force Re-scrape: %s\n", config->force_rescrape ? "Yes" : "No");
    printf("User Agent: %s\n", config->user_agent ? config->user_agent : "Default");
    printf("Request Timeout: %d seconds\n", config->request_timeout);
    printf("Retry Count: %d\n", config->retry_count);
    printf("Retry Delay: %d seconds\n", config->retry_delay);
    printf("============================\n\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse command line arguments
    char *url = NULL;
    int analyze_mode = 0;
    int trends_mode = 0;
    int trends_limit = 10;
    int config_mode = 0;
    int force_mode = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--analyze") == 0) {
            if (i + 1 < argc) {
                url = argv[++i];
                analyze_mode = 1;
            } else {
                fprintf(stderr, "Error: Missing URL for analysis\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--trends") == 0) {
            trends_mode = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                trends_limit = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            config_mode = 1;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) {
            force_mode = 1;
            scraper_config_t *config = get_scraper_config();
            if (config) {
                config->force_rescrape = 1;
                set_scraper_config(config);
                free(config->user_agent);
                free(config);
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--depth") == 0) {
            if (i + 1 < argc) {
                int depth = atoi(argv[++i]);
                scraper_config_t *config = get_scraper_config();
                if (config) {
                    config->max_depth = depth;
                    set_scraper_config(config);
                    free(config->user_agent);
                    free(config);
                }
            } else {
                fprintf(stderr, "Error: Missing value for depth\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pages") == 0) {
            if (i + 1 < argc) {
                int pages = atoi(argv[++i]);
                scraper_config_t *config = get_scraper_config();
                if (config) {
                    config->max_pages = pages;
                    set_scraper_config(config);
                    free(config->user_agent);
                    free(config);
                }
            } else {
                fprintf(stderr, "Error: Missing value for pages\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--memory") == 0) {
            if (i + 1 < argc) {
                int memory = atoi(argv[++i]);
                scraper_config_t *config = get_scraper_config();
                if (config) {
                    config->max_memory_mb = memory;
                    set_scraper_config(config);
                    free(config->user_agent);
                    free(config);
                }
            } else {
                fprintf(stderr, "Error: Missing value for memory\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--javascript") == 0) {
            scraper_config_t *config = get_scraper_config();
            if (config) {
                config->use_javascript = 1;
                set_scraper_config(config);
                free(config->user_agent);
                free(config);
            }
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--no-robots") == 0) {
            scraper_config_t *config = get_scraper_config();
            if (config) {
                config->respect_robots = 0;
                set_scraper_config(config);
                free(config->user_agent);
                free(config);
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            // Set verbose mode (implement as needed)
        } else if (argv[i][0] != '-') {
            // Assume this is the URL
            url = argv[i];
        }
    }
    
    // Initialize scraper
    if (init_scraper() != 0) {
        fprintf(stderr, "Failed to initialize scraper\n");
        return 1;
    }
    
    // Handle different modes
    if (config_mode) {
        scraper_config_t *config = get_scraper_config();
        print_config(config);
        free(config->user_agent);
        free(config);
    } else if (trends_mode) {
        trend_data_t **trends = get_trending_topics(trends_limit);
        if (trends) {
            print_trends(trends, trends_limit);
            free_trend_data(trends, trends_limit);
        }
    } else if (analyze_mode && url) {
        content_analysis_t *analysis = analyze_url_content(url);
        if (analysis) {
            print_analysis(analysis);
            free_content_analysis(analysis);
        }
    } else if (url) {
        // Regular scraping mode
        LOG_INFO("Starting web scraper with URL: %s", url);
        
        // Create URL task
        url_task_t *task = malloc(sizeof(url_task_t));
        if (!task) {
            LOG_ERROR("Failed to allocate memory for URL task");
            cleanup_scraper();
            return 1;
        }
        
        task->url = strdup(url);
        if (!task->url) {
            LOG_ERROR("Failed to allocate memory for URL string");
            free(task);
            cleanup_scraper();
            return 1;
        }
        
        task->priority = 1;
        task->depth = 0;
        task->parent_url = NULL;
        
        LOG_INFO("Adding URL task to thread pool: %s", task->url);
        if (!thread_pool_add_task(scraper_pool, process_url_thread, task)) {
            LOG_ERROR("Failed to add URL task to thread pool");
            free(task->url);
            free(task);
            cleanup_scraper();
            return 1;
        }
        LOG_INFO("URL task added to thread pool successfully");
        
        // Wait for task to complete
        LOG_INFO("Waiting for task to complete...");
        while (thread_pool_get_queue_size(scraper_pool) > 0) {
            usleep(100000); // Sleep for 100ms
        }
        LOG_INFO("Task completed");
    } else {
        fprintf(stderr, "Error: No URL provided\n");
        print_usage(argv[0]);
        cleanup_scraper();
        return 1;
    }
    
    // Cleanup
    cleanup_scraper();
    LOG_INFO("Scraper cleanup completed");
    return 0;
} 