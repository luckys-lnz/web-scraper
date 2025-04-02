#include "fetch_url.h"
#include "write_callback.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Fetches the content of a URL using libcurl.
 */
void fetch_url(const char *url, struct Memory *chunk) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Failed to initialize CURL\n");
    return;
  }

  chunk->response = malloc(1);
  chunk->size = 0;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // Timeout for safety

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "CURL error: %s\n", curl_easy_strerror(res));
  }

  curl_easy_cleanup(curl);
}
