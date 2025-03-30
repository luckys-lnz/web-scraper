#include "scraper.h"
#include "redis_helper.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 3

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Web scraping worker function.
 */
void *scrape(void *arg) {
  while (1) {
    char *url = fetch_url_from_queue();
    if (!url) {
      free(url);
      break; // No more URLs
    }

    if (is_visited(url)) {
      pthread_mutex_lock(&print_mutex);
      printf("Skipping %s (already visited)\n", url);
      pthread_mutex_unlock(&print_mutex);
      free(url);
      continue;
    }

    struct Memory chunk = {NULL, 0};
    fetch_url(url, &chunk);

    if (chunk.response) {
      pthread_mutex_lock(&print_mutex);
      printf("\n=== Scraping: %s ===\n", url);
      extract_title(chunk.response);
      extract_meta(chunk.response);
      extract_hrefs(chunk.response);
      pthread_mutex_unlock(&print_mutex);

      mark_visited(url);
      free(chunk.response);
    }

    free(url);
  }
  pthread_exit(NULL);
}

int main() {
  pthread_t threads[NUM_THREADS];

  // Initialize dependencies
  curl_global_init(CURL_GLOBAL_ALL);
  init_redis();

  // Seed URLs
  const char *seed_urls[] = {"https://archlinux.org/",
                             "https://www.google.com/", "https://github.com/"};
  for (int i = 0; i < NUM_THREADS; i++) {
    push_url_to_queue(seed_urls[i]);
  }

  // Start scraping threads
  for (int i = 0; i < NUM_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, scrape, NULL) != 0) {
      fprintf(stderr, "Error creating thread\n");
      return EXIT_FAILURE;
    }
  }

  // Wait for threads to finish
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  // Cleanup
  close_redis();
  curl_global_cleanup();
  return EXIT_SUCCESS;
}
