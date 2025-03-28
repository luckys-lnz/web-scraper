#include "scraper.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 3

pthread_mutex_t print_mutex; // Mutex for printing

void *scrape(void *arg) {
  const char *url = (const char *)arg;
  struct Memory chunk = {NULL, 0};

  fetch_url(url, &chunk);

  if (chunk.response) {
    pthread_mutex_lock(&print_mutex);
    printf("\n=== Scraping: %s ===\n", url);
    extract_title(chunk.response);
    extract_meta(chunk.response);
    extract_hrefs(chunk.response);
    pthread_mutex_unlock(&print_mutex);

    free(chunk.response);
  }

  pthread_exit(NULL);
}

int main() {
  pthread_t threads[NUM_THREADS];
  const char *urls[] = {"https://archlinux.org/", "https://www.google.com/",
                        "https://github.com/"};

  curl_global_init(CURL_GLOBAL_ALL);      // Initialize CURL once
  pthread_mutex_init(&print_mutex, NULL); // Initialize mutex

  for (int i = 0; i < NUM_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, scrape, (void *)urls[i]) != 0) {
      fprintf(stderr, "Error creating thread\n");
      return 1;
    }
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  pthread_mutex_destroy(&print_mutex);
  curl_global_cleanup();
  return 0;
}
