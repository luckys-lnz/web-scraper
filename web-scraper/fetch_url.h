#ifndef FETCH_URL_H
#define FETCH_URL_H

#include "scraper.h" // For struct Memory
#include <curl/curl.h>

/**
 * Fetches the content of a URL and stores it in a dynamically allocated buffer.
 *
 * @param url The URL to fetch.
 * @param chunk Pointer to a Memory struct to store the response.
 */
void fetch_url(const char *url, struct Memory *chunk);

#endif // FETCH_URL_H
