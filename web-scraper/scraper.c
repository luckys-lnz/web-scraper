#include <curl/curl.h>
#include <curl/easy.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Struct to store response
struct Memory {
  char *response;
  size_t size;
};

// Callback function for handling response data
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total_size = size * nmemb;
  struct Memory *mem = (struct Memory *)userp;

  // printf("%.*s", (int)(size * nmemb), (char *)contents);
  // return size * nmemb;
  // Allocate memory dynamically

  char *ptr = realloc(mem->response, mem->size + total_size + 1);
  if (ptr == NULL) {
    printf("Not enough memory\n");
    return 0;
  }

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), contents, total_size);
  mem->size += total_size;
  mem->response[mem->size] = '\0'; // Null-terminate the string

  return total_size;
}

// Function to extract <title> content
void extract_title(const char *html) {
  // check for NULL
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
      char title[title_length + 1];
      strncpy(title, title_start, title_length);
      title[title_length] = '\0'; // Null-terminate
      printf("Title: %s\n", title);
    } else {
      printf("No closing </title> tag found.\n");
    }
  } else {
    printf("No <title> tag found in the response.\n");
  }
}

void extract_meta(const char *html) {
  // check if html is NULL
  if (html == NULL)
    return;

  regex_t regex;
  regmatch_t matches[2];
  const char *pattern = "meta\\s+([^>]+)>";

  // compile regex
  if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
    printf("could not compile regex\n");
    return;
  }

  const char *cursor = html;

  while (regexec(&regex, cursor, 2, matches, 0) == 0) {
    int start = matches[1].rm_so;
    int end = matches[1].rm_eo;

    printf("meta: %.*s\n", end - start, cursor + start);
    // move cursor forward
    cursor += matches[0].rm_eo;
  }
  regfree(&regex);
}

// extract hrefs
void extract_hrefs(const char *html) {
  if (html == NULL)
    return;

  regex_t regex;
  regmatch_t matches[2];

  // compile regex pattern
  if (regcomp(&regex, "<a\\s+href=[\"']([^\"']+)[\"']", REG_EXTENDED) != 0) {
    printf("Could not compile regex\n");
    return;
  }

  const char *cursor = html;

  while (regexec(&regex, cursor, 2, matches, 0) == 0) {
    int start = matches[1].rm_so;
    int end = matches[1].rm_eo;

    printf("Found URL: %.*s\n", end - start, cursor + start);

    // move cursor forward
    cursor += matches[0].rm_eo;
  }

  regfree(&regex);
}

int main() {
  CURL *curl;
  CURLcode res;
  struct Memory chunk = {NULL, 0};

  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.google.com");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    // curl_easy_perform(curl);

    res = curl_easy_perform(curl);

    // use DRY principle to handle multiple if statements

    // // grab title, meta, hrefs

    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
    } else {
      extract_title(chunk.response); // Extract title
      extract_meta(chunk.response);  // Extract meta
      extract_hrefs(chunk.response); // Extract hrefs
    }

    free(chunk.response);
    curl_easy_cleanup(curl);
  }

  curl_global_cleanup();
  return 0;
}
