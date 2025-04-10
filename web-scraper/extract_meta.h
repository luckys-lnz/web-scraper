#ifndef EXTRACT_META_H
#define EXTRACT_META_H

#include "scraper.h"

/**
 * Extracts and prints all <meta> tags (both name/content and property/content).
 *
 * @param html Pointer to the HTML content.
 */
void extract_meta(const char *html);

#endif // EXTRACT_META_H 