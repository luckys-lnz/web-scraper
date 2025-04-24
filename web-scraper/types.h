#ifndef TYPES_H
#define TYPES_H

// URL processing task data
typedef struct {
    char *url;
    int priority;
    int depth;  // Crawling depth
    char *parent_url;  // URL that led to this one
} url_task_t;

// Content analysis results
typedef struct {
    char *title;
    char *description;
    char *keywords;
    char *author;
    char *publish_date;
    char *main_content;
    char *language;
    float sentiment_score;  // -1.0 to 1.0
    char **topics;  // Array of detected topics
    int topic_count;
    char **entities;  // Named entities (people, organizations, etc.)
    int entity_count;
    char **categories;  // Content categories
    int category_count;
} content_analysis_t;

// Trend data
typedef struct {
    char *topic;
    int frequency;
    float growth_rate;  // Percentage growth over time
    char **related_topics;
    int related_topic_count;
    char **sources;  // URLs where this trend was detected
    int source_count;
} trend_data_t;

// Scraping configuration
typedef struct {
    int max_depth;
    int max_pages;
    int max_memory_mb;
    int respect_robots;
    int use_javascript;
    int extract_media;
    int analyze_content;
    int track_trends;
    int force_rescrape;  // Force re-scraping of already visited URLs
    char *user_agent;
    int request_timeout;
    int retry_count;
    int retry_delay;
} scraper_config_t;

#endif // TYPES_H 