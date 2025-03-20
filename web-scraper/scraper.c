#include "scraper.h"
#include <curl/curl.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Callback function for handling HTTP response data in a CURL request.
 *
 * This function is called by libcurl when data is received from the server.
 * It dynamically reallocates memory to store the received data and appends
 * it to the existing buffer in the `Memory` struct.
 *
 * @param contents Pointer to the received data chunk.
 * @param size Size of each data element.
 * @param nmemb Number of elements received.
 * @param userp Pointer to the user-defined Memory struct where data is stored.
 * @return The total size of data processed (size * nmemb). Returns 0 if memory
 *         allocation fails.
 *
 * @note The function dynamically expands the `response` buffer in `Memory`
 *       using `realloc()`. The caller is responsible for freeing
 *       `mem->response` when it is no longer needed.
 * @note The function ensures that the response string is always
 * null-terminated.
 * @note If `realloc()` fails, the previously allocated memory is freed, and
 *       an error message is printed.
 */
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total_size = size * nmemb;
  struct Memory *mem = (struct Memory *)userp;

  char *ptr = realloc(mem->response, mem->size + total_size + 1);
  if (!ptr) {
    free(mem->response);
    printf("Not enough memory\n");
    return 0;
  }

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), contents, total_size);
  mem->size += total_size;
  mem->response[mem->size] = '\0'; // Null-terminate the string

  return total_size;
}

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

/**
 * Extracts and prints the content inside the <title> tag from the given HTML.
 *
 * @param html: Pointer to the HTML content.
 */
void extract_title(const char *html) {
  if (html == NULL) {
    printf("No HTML data available.\n");
    return;
  }

  char *title_start = strstr(html, "<title>");
  if (title_start) {
    title_start += 7; // Move past "<title>"
    char *title_end = strstr(title_start, "</title>");
    if (title_end) {
      size_t title_length = title_end - title_start;
      char *title = (char *)malloc(title_length + 1);
      if (title) {
        strncpy(title, title_start, title_length);
        title[title_length] = '\0'; // Null-terminate
        printf("Title: %s\n", title);
        free(title);
      }
    } else {
      printf("No closing </title> tag found.\n");
    }
  } else {
    printf("No <title> tag found in the response.\n");
  }
}

/**
 * Extracts and prints all <meta> tags from the given HTML.
 *
 * @param html: Pointer to the HTML content.
 */
void extract_meta(const char *html) {
  if (html == NULL)
    return;

  regex_t regex;
  regmatch_t matches[3];
  const char *pattern =
      "<meta\\s+[^>]*name=[\"']([^\"']+)[\"'][^>]*content=[\"']([^\"']+)[\"']";

  if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
    printf("Could not compile regex\n");
    return;
  }

  const char *cursor = html;

  while (regexec(&regex, cursor, 3, matches, 0) == 0) {
    printf("Meta name: %.*s\n", matches[1].rm_eo - matches[1].rm_so,
           cursor + matches[1].rm_so);
    printf("Meta content: %.*s\n", matches[2].rm_eo - matches[2].rm_so,
           cursor + matches[2].rm_so);
    cursor += matches[0].rm_eo;
  }

  regfree(&regex);
}

/**
 * Extracts and prints all hyperlinks (<a href="...") from the given HTML.
 *
 * @param html: Pointer to the HTML content.
 */
void extract_hrefs(const char *html) {
  if (html == NULL)
    return;

  regex_t regex;
  regmatch_t matches[2];
  const char *pattern = "<a\\s+[^>]*href=[\"']([^\"']+)[\"']";

  if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
    printf("Could not compile regex\n");
    return;
  }

  const char *cursor = html;

  while (regexec(&regex, cursor, 2, matches, 0) == 0) {
    int start = matches[1].rm_so;
    int end = matches[1].rm_eo;
    printf("Found URL: %.*s\n", end - start, cursor + start);
    cursor += matches[0].rm_eo;
  }

  regfree(&regex);
}
