#include <curl/curl.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Struct to store response data dynamically
struct Memory {
  char
      *response; // Pointer to dynamically allocated memory storing the response
  size_t size;   // Size of the response data
};

/**
 * Callback function for handling response data received by libcurl.
 * This function dynamically allocates memory to store the response.
 *
 * @param contents Pointer to the received data.
 * @param size Size of one data chunk.
 * @param nmemb Number of data chunks.
 * @param userp Pointer to user-defined data (Memory struct in this case).
 * @return The total size of the received data (size * nmemb).
 */
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total_size = size * nmemb;
  struct Memory *mem = (struct Memory *)userp;

  // Reallocate memory to accommodate new data
  char *ptr = realloc(mem->response, mem->size + total_size + 1);
  if (!ptr) {
    printf("Not enough memory\n");
    return 0; // Return 0 to signal failure
  }

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), contents, total_size);
  mem->size += total_size;
  mem->response[mem->size] = '\0'; // Null-terminate the string

  return total_size;
}

/**
 * Extracts and prints the content inside the <title> tag from the given HTML.
 *
 * @param html Pointer to the HTML content.
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
 * @param html Pointer to the HTML content.
 */
void extract_meta(const char *html) {
  if (html == NULL)
    return;

  regex_t regex;
  regmatch_t matches[1];
  const char *pattern = "<meta\\s+[^>]*>";

  if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
    printf("Could not compile regex\n");
    return;
  }

  const char *cursor = html;

  while (regexec(&regex, cursor, 1, matches, 0) == 0) {
    int start = matches[0].rm_so;
    int end = matches[0].rm_eo;
    printf("Meta tag: %.*s\n", end - start, cursor + start);
    cursor += matches[0].rm_eo;
  }

  regfree(&regex);
}

/**
 * Extracts and prints all hyperlinks (<a href="...") from the given HTML.
 *
 * @param html Pointer to the HTML content.
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

int main() {
  CURL *curl;
  CURLcode res;
  struct Memory chunk = {NULL, 0}; // Initialize response struct

  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.google.com");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    res = curl_easy_perform(curl);

    if (res == CURLE_OK && chunk.response) {
      extract_title(chunk.response);
      extract_meta(chunk.response);
      extract_hrefs(chunk.response);
    } else {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
    }

    free(chunk.response);
    curl_easy_cleanup(curl);
  }

  curl_global_cleanup();
  return 0;
}

