#include "scraper.h"

/**
 * @brief Fetches the HTML content of a given URL and stores it in a Memory
 * struct.
 *
 * This function initializes a CURL session, performs an HTTP GET request to
 * fetch the HTML content of the specified URL, and stores the response in the
 * provided Memory struct. The function uses the `write_callback` function to
 * handle the received data.
 *
 * @param url: The URL to fetch data from.
 * @param chunk: Pointer to a Memory struct that will store the response data.
 *
 * @note The caller is responsible for freeing `chunk->response` after use to
 *       prevent memory leaks.
 * @note The function initializes and cleans up CURL globally, which may not be
 *       ideal for repeated calls in a multi-threaded environment. Consider
 *       initializing CURL globally once in `main()` if making multiple
 * requests.
 */

void fetch_url(const char *url, struct Memory *chunk) {
  CURL *curl;
  CURLcode res;

  curl_global_init(CURL_GLOBAL_ALL); // Initialize CURL globally
  curl = curl_easy_init();           // Initialize CURL session

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);

    res = curl_easy_perform(curl); // Perform the request

    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl); // Cleanup CURL session
  }

  curl_global_cleanup(); // Cleanup CURL globally
}
