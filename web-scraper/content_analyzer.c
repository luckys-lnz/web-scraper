#include "content_analyzer.h"
#include "logger.h"
#include "redis_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <time.h>
#include <regex.h>

// Redis key prefixes
#define ANALYSIS_KEY_PREFIX "analysis:"
#define TREND_KEY_PREFIX "trend:"
#define TREND_COUNT_KEY "trend:count"

// Global variables
extern redisContext *redis_ctx;

// Initialize the content analyzer
int init_content_analyzer(redisContext *ctx) {
    if (!ctx) {
        LOG_ERROR("Redis context is NULL");
        return -1;
    }
    
    redis_ctx = ctx;
    LOG_INFO("Content analyzer initialized");
    return 0;
}

// Extract text content from HTML
char *extract_text_content(const char *html) {
    if (!html) return NULL;
    
    // Cast html to xmlChar* for htmlReadDoc
    htmlDocPtr doc = htmlReadDoc((const xmlChar *)html, NULL, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) return NULL;
    
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        LOG_ERROR("Failed to get root element");
        xmlFreeDoc(doc);
        return NULL;
    }
    
    // Create a buffer for the text content
    char *text = malloc(1);
    if (!text) {
        LOG_ERROR("Failed to allocate memory for text content");
        xmlFreeDoc(doc);
        return NULL;
    }
    text[0] = '\0';
    
    // Extract text from all text nodes
    xmlNodePtr node = root;
    while (node) {
        if (node->type == XML_ELEMENT_NODE) {
            // Skip script and style elements
            if (xmlStrcmp(node->name, (const xmlChar *)"script") == 0 ||
                xmlStrcmp(node->name, (const xmlChar *)"style") == 0) {
                node = node->next;
                continue;
            }
            
            // Process child nodes
            xmlNodePtr child = node->children;
            while (child) {
                if (child->type == XML_TEXT_NODE && child->content) {
                    // Append text content
                    size_t text_len = strlen(text);
                    size_t content_len = strlen((char *)child->content);
                    char *new_text = realloc(text, text_len + content_len + 1);
                    if (!new_text) {
                        LOG_ERROR("Failed to allocate memory for text content");
                        free(text);
                        xmlFreeDoc(doc);
                        return NULL;
                    }
                    text = new_text;
                    strcat(text, (char *)child->content);
                    strcat(text, " ");
                }
                child = child->next;
            }
        }
        node = node->next;
    }
    
    xmlFreeDoc(doc);
    return text;
}

// Extract title from HTML
char *extract_title_from_html(const char *html) {
    if (!html) return NULL;
    
    // Cast html to xmlChar* for htmlReadDoc
    htmlDocPtr doc = htmlReadDoc((const xmlChar *)html, NULL, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) return NULL;
    
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (!xpathCtx) {
        LOG_ERROR("Failed to create XPath context");
        xmlFreeDoc(doc);
        return NULL;
    }
    
    // Query for title
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar *)"//title", xpathCtx);
    if (!xpathObj) {
        LOG_ERROR("Failed to evaluate XPath expression");
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return NULL;
    }
    
    char *title = NULL;
    if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
        xmlNodePtr node = xpathObj->nodesetval->nodeTab[0];
        if (node->children && node->children->content) {
            title = strdup((char *)node->children->content);
        }
    }
    
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    
    return title;
}

// Extract meta description from HTML
char *extract_meta_description(const char *html) {
    if (!html) return NULL;
    
    // Cast html to xmlChar* for htmlReadDoc
    htmlDocPtr doc = htmlReadDoc((const xmlChar *)html, NULL, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) return NULL;
    
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (!xpathCtx) {
        LOG_ERROR("Failed to create XPath context");
        xmlFreeDoc(doc);
        return NULL;
    }
    
    // Query for meta description
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar *)"//meta[@name='description']/@content", xpathCtx);
    if (!xpathObj) {
        LOG_ERROR("Failed to evaluate XPath expression");
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return NULL;
    }
    
    char *description = NULL;
    if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
        xmlNodePtr node = xpathObj->nodesetval->nodeTab[0];
        if (node->content) {
            description = strdup((char *)node->content);
        }
    }
    
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    
    return description;
}

// Extract meta keywords from HTML
char *extract_meta_keywords(const char *html) {
    if (!html) return NULL;
    
    // Cast html to xmlChar* for htmlReadDoc
    htmlDocPtr doc = htmlReadDoc((const xmlChar *)html, NULL, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) return NULL;
    
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (!xpathCtx) {
        LOG_ERROR("Failed to create XPath context");
        xmlFreeDoc(doc);
        return NULL;
    }
    
    // Query for meta keywords
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar *)"//meta[@name='keywords']/@content", xpathCtx);
    if (!xpathObj) {
        LOG_ERROR("Failed to evaluate XPath expression");
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return NULL;
    }
    
    char *keywords = NULL;
    if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
        xmlNodePtr node = xpathObj->nodesetval->nodeTab[0];
        if (node->content) {
            keywords = strdup((char *)node->content);
        }
    }
    
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    
    return keywords;
}

// Simple sentiment analysis (placeholder for more sophisticated AI)
static float analyze_sentiment(const char *text) {
    if (!text) {
        return 0.0f;
    }
    
    // This is a very simple sentiment analysis
    // In a real implementation, this would use a machine learning model
    int positive_words = 0;
    int negative_words = 0;
    
    // Simple positive words
    const char *positive[] = {"good", "great", "excellent", "amazing", "wonderful", "best", "love", "happy", "joy", "success"};
    int positive_count = 10;
    
    // Simple negative words
    const char *negative[] = {"bad", "terrible", "awful", "worst", "hate", "sad", "angry", "failure", "poor", "disaster"};
    int negative_count = 10;
    
    // Count occurrences
    for (int i = 0; i < positive_count; i++) {
        const char *pos = text;
        while ((pos = strstr(pos, positive[i])) != NULL) {
            positive_words++;
            pos += strlen(positive[i]);
        }
    }
    
    for (int i = 0; i < negative_count; i++) {
        const char *pos = text;
        while ((pos = strstr(pos, negative[i])) != NULL) {
            negative_words++;
            pos += strlen(negative[i]);
        }
    }
    
    // Calculate sentiment score (-1.0 to 1.0)
    int total = positive_words + negative_words;
    if (total == 0) {
        return 0.0f;
    }
    
    return (float)(positive_words - negative_words) / total;
}

// Analyze HTML content and extract structured data
content_analysis_t *analyze_content(const char *html, const char *url) {
    (void)url;  // Mark url parameter as intentionally unused
    
    if (!html) return NULL;
    
    content_analysis_t *analysis = malloc(sizeof(content_analysis_t));
    if (!analysis) {
        LOG_ERROR("Failed to allocate memory for content analysis");
        return NULL;
    }
    
    // Initialize all fields to NULL or 0
    memset(analysis, 0, sizeof(content_analysis_t));
    
    // Extract basic metadata
    analysis->title = extract_title_from_html(html);
    analysis->description = extract_meta_description(html);
    analysis->keywords = extract_meta_keywords(html);
    
    // Extract text content for analysis
    char *text_content = extract_text_content(html);
    if (text_content) {
        // Simple sentiment analysis
        analysis->sentiment_score = analyze_sentiment(text_content);
        
        // In a real implementation, we would use AI to extract:
        // - Topics
        // - Entities
        // - Categories
        // - Language
        // - Author
        // - Publish date
        
        // For now, we'll just set some placeholder values
        analysis->language = strdup("en");  // Assume English
        
        // Free the text content
        free(text_content);
    }
    
    return analysis;
}

// Free a content_analysis_t structure
void free_content_analysis(content_analysis_t *analysis) {
    if (!analysis) {
        return;
    }
    
    free(analysis->title);
    free(analysis->description);
    free(analysis->keywords);
    free(analysis->author);
    free(analysis->publish_date);
    free(analysis->main_content);
    free(analysis->language);
    
    // Free topics
    if (analysis->topics) {
        for (int i = 0; i < analysis->topic_count; i++) {
            free(analysis->topics[i]);
        }
        free(analysis->topics);
    }
    
    // Free entities
    if (analysis->entities) {
        for (int i = 0; i < analysis->entity_count; i++) {
            free(analysis->entities[i]);
        }
        free(analysis->entities);
    }
    
    // Free categories
    if (analysis->categories) {
        for (int i = 0; i < analysis->category_count; i++) {
            free(analysis->categories[i]);
        }
        free(analysis->categories);
    }
    
    free(analysis);
}

// Store analysis results in Redis
int store_analysis_results(redisContext *ctx, const char *url, content_analysis_t *analysis) {
    if (!ctx || !url || !analysis) {
        LOG_ERROR("Invalid parameters for storing analysis results");
        return -1;
    }
    
    // Create Redis key
    char key[1024];
    snprintf(key, sizeof(key), "%s%s", ANALYSIS_KEY_PREFIX, url);
    
    // Store basic metadata
    redisReply *reply;
    
    // Title
    if (analysis->title) {
        reply = redisCommand(ctx, "HSET %s title %s", key, analysis->title);
        if (!reply) {
            LOG_ERROR("Failed to store title in Redis");
            return -1;
        }
        freeReplyObject(reply);
    }
    
    // Description
    if (analysis->description) {
        reply = redisCommand(ctx, "HSET %s description %s", key, analysis->description);
        if (!reply) {
            LOG_ERROR("Failed to store description in Redis");
            return -1;
        }
        freeReplyObject(reply);
    }
    
    // Keywords
    if (analysis->keywords) {
        reply = redisCommand(ctx, "HSET %s keywords %s", key, analysis->keywords);
        if (!reply) {
            LOG_ERROR("Failed to store keywords in Redis");
            return -1;
        }
        freeReplyObject(reply);
    }
    
    // Sentiment score
    reply = redisCommand(ctx, "HSET %s sentiment %f", key, analysis->sentiment_score);
    if (!reply) {
        LOG_ERROR("Failed to store sentiment score in Redis");
        return -1;
    }
    freeReplyObject(reply);
    
    // Language
    if (analysis->language) {
        reply = redisCommand(ctx, "HSET %s language %s", key, analysis->language);
        if (!reply) {
            LOG_ERROR("Failed to store language in Redis");
            return -1;
        }
        freeReplyObject(reply);
    }
    
    // Store timestamp
    time_t now = time(NULL);
    reply = redisCommand(ctx, "HSET %s timestamp %ld", key, now);
    if (!reply) {
        LOG_ERROR("Failed to store timestamp in Redis");
        return -1;
    }
    freeReplyObject(reply);
    
    LOG_INFO("Stored analysis results for URL: %s", url);
    return 0;
}

// Retrieve analysis results from Redis
content_analysis_t *get_analysis_results(redisContext *ctx, const char *url) {
    if (!ctx || !url) {
        LOG_ERROR("Invalid parameters for retrieving analysis results");
        return NULL;
    }
    
    // Create Redis key
    char key[1024];
    snprintf(key, sizeof(key), "%s%s", ANALYSIS_KEY_PREFIX, url);
    
    // Check if analysis exists
    redisReply *reply = redisCommand(ctx, "EXISTS %s", key);
    if (!reply) {
        LOG_ERROR("Failed to check if analysis exists in Redis");
        return NULL;
    }
    
    if (reply->integer == 0) {
        freeReplyObject(reply);
        LOG_INFO("No analysis results found for URL: %s", url);
        return NULL;
    }
    
    freeReplyObject(reply);
    
    // Create analysis structure
    content_analysis_t *analysis = malloc(sizeof(content_analysis_t));
    if (!analysis) {
        LOG_ERROR("Failed to allocate memory for content analysis");
        return NULL;
    }
    
    // Initialize all fields to NULL or 0
    memset(analysis, 0, sizeof(content_analysis_t));
    
    // Retrieve title
    reply = redisCommand(ctx, "HGET %s title", key);
    if (reply && reply->type == REDIS_REPLY_STRING) {
        analysis->title = strdup(reply->str);
    }
    freeReplyObject(reply);
    
    // Retrieve description
    reply = redisCommand(ctx, "HGET %s description", key);
    if (reply && reply->type == REDIS_REPLY_STRING) {
        analysis->description = strdup(reply->str);
    }
    freeReplyObject(reply);
    
    // Retrieve keywords
    reply = redisCommand(ctx, "HGET %s keywords", key);
    if (reply && reply->type == REDIS_REPLY_STRING) {
        analysis->keywords = strdup(reply->str);
    }
    freeReplyObject(reply);
    
    // Retrieve sentiment score
    reply = redisCommand(ctx, "HGET %s sentiment", key);
    if (reply && reply->type == REDIS_REPLY_STRING) {
        analysis->sentiment_score = atof(reply->str);
    }
    freeReplyObject(reply);
    
    // Retrieve language
    reply = redisCommand(ctx, "HGET %s language", key);
    if (reply && reply->type == REDIS_REPLY_STRING) {
        analysis->language = strdup(reply->str);
    }
    freeReplyObject(reply);
    
    LOG_INFO("Retrieved analysis results for URL: %s", url);
    return analysis;
}

// Detect trends from a collection of analyzed content
trend_data_t **detect_trends(redisContext *ctx, int limit) {
    if (!ctx || limit <= 0) {
        LOG_ERROR("Invalid parameters for detecting trends");
        return NULL;
    }
    
    // In a real implementation, this would use AI to detect trends
    // For now, we'll just return a placeholder
    
    // Allocate memory for trends
    trend_data_t **trends = malloc(sizeof(trend_data_t *) * limit);
    if (!trends) {
        LOG_ERROR("Failed to allocate memory for trends");
        return NULL;
    }
    
    // Create a placeholder trend
    trends[0] = malloc(sizeof(trend_data_t));
    if (!trends[0]) {
        LOG_ERROR("Failed to allocate memory for trend");
        free(trends);
        return NULL;
    }
    
    // Initialize trend
    trends[0]->topic = strdup("AI");
    trends[0]->frequency = 42;
    trends[0]->growth_rate = 15.5f;
    trends[0]->related_topic_count = 0;
    trends[0]->related_topics = NULL;
    trends[0]->source_count = 0;
    trends[0]->sources = NULL;
    
    // Set the rest to NULL
    for (int i = 1; i < limit; i++) {
        trends[i] = NULL;
    }
    
    LOG_INFO("Detected trends");
    return trends;
}

// Free an array of trend_data_t structures
void free_trend_data(trend_data_t **trends, int count) {
    if (!trends) {
        return;
    }
    
    for (int i = 0; i < count; i++) {
        if (trends[i]) {
            free(trends[i]->topic);
            
            // Free related topics
            if (trends[i]->related_topics) {
                for (int j = 0; j < trends[i]->related_topic_count; j++) {
                    free(trends[i]->related_topics[j]);
                }
                free(trends[i]->related_topics);
            }
            
            // Free sources
            if (trends[i]->sources) {
                for (int j = 0; j < trends[i]->source_count; j++) {
                    free(trends[i]->sources[j]);
                }
                free(trends[i]->sources);
            }
            
            free(trends[i]);
        }
    }
    
    free(trends);
}

// Clean up the content analyzer
void cleanup_content_analyzer(void) {
    redis_ctx = NULL;
    LOG_INFO("Content analyzer cleaned up");
}