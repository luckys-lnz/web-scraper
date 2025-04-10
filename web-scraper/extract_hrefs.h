#ifndef EXTRACT_HREFS_H
#define EXTRACT_HREFS_H

#include "scraper.h"
#include "redis_helper.h"

/**
 * Extracts and processes all hyperlinks (<a href="...") from the given HTML.
 *
 * @param html Pointer to the HTML content.
 * @param base_url The base URL of the page.
 */
void extract_hrefs(const char *html, const char *base_url);

#endif // EXTRACT_HREFS_H 