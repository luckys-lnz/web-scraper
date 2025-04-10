#ifndef EXTRACT_TITLE_H
#define EXTRACT_TITLE_H

#include "scraper.h"

/**
 * Extracts and prints the content inside the <title> tag from the given HTML.
 *
 * @param html Pointer to the HTML content.
 */
void extract_title(const char *html);

#endif // EXTRACT_TITLE_H 