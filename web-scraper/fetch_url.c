#include "scraper.h"

/**
 * Fetches the HTML content of a given URL and stores it in a Memory struct.
 *
 * @param url: The URL to fetch data from.
 * @param chunk: Pointer to a Memory struct that will store the response data.
 *
 * @note The caller must free `chunk->response` after use.
 */
void fetch_url(const char *url, struct Memory *chunk) {
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init(); // Initialize CURL session

  if (!curl) {
    fprintf(stderr, "Failed to initialize CURL\n");
    return;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // Follow redirects
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // Handle gzip & deflate

  res = curl_easy_perform(curl); // Perform the request

  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
  }

  curl_easy_cleanup(curl); // Cleanup CURL session
}

