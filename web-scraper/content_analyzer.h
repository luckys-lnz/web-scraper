#ifndef CONTENT_ANALYZER_H
#define CONTENT_ANALYZER_H

#include "types.h"
#include <hiredis/hiredis.h>

// Initialize the content analyzer
// Returns 0 on success, -1 on failure
int init_content_analyzer(redisContext *ctx);

// Analyze HTML content and extract structured data
// Returns a content_analysis_t structure with the analysis results
// Caller is responsible for freeing the returned structure
content_analysis_t *analyze_content(const char *html, const char *url);

// Free a content_analysis_t structure
void free_content_analysis(content_analysis_t *analysis);

// Store analysis results in Redis
// Returns 0 on success, -1 on failure
int store_analysis_results(redisContext *ctx, const char *url, content_analysis_t *analysis);

// Retrieve analysis results from Redis
// Returns a content_analysis_t structure with the analysis results
// Caller is responsible for freeing the returned structure
content_analysis_t *get_analysis_results(redisContext *ctx, const char *url);

// Detect trends from a collection of analyzed content
// Returns an array of trend_data_t structures
// Caller is responsible for freeing the returned array
trend_data_t **detect_trends(redisContext *ctx, int limit);

// Free an array of trend_data_t structures
void free_trend_data(trend_data_t **trends, int count);

// Clean up the content analyzer
void cleanup_content_analyzer(void);

#endif // CONTENT_ANALYZER_H 