#include "scraper.h"
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
  size_t realsize = size * nmemb;
  struct Memory *mem = (struct Memory *)userp;

  char *ptr = realloc(mem->response, mem->size + realsize + 1);
  if (!ptr) {
    fprintf(stderr, "Memory allocation failed in write_callback\n");
    return 0;
  }

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;

  return realsize;
}
