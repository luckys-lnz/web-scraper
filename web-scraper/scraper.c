#include "scraper.h"
#include "fetch_url.h"
#include "redis_helper.h"
#include "robots_parser.h"
#include <pthread.h>

#define NUM_THREADS 3

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to extract base URL from full URL
void split_url(const char *url, char *base_url, char *target_path) {
  const char *slash = strchr(url + 8, '/');
  if (slash) {
    size_t base_len = slash - url;
    strncpy(base_url, url, base_len);
    base_url[base_len] = '\0';
    strcpy(target_path, slash);
  } else {
    strcpy(base_url, url);
    target_path[0] = '/';
    target_path[1] = '\0';
  }
}

void *scrape(void *arg) {
  while (1) {
    char *url = fetch_url_from_queue();
    if (!url) {
      free(url);
      break;
    }

    char base_url[256], target_path[1024];
    split_url(url, base_url, target_path); // Extract parts

    if (!is_crawl_allowed(base_url, target_path)) {
      pthread_mutex_lock(&print_mutex);
      printf("Skipping %s (disallowed by robots.txt)\n", url);
      pthread_mutex_unlock(&print_mutex);
      free(url);
      continue;
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
      extract_hrefs(chunk.response, url);
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

  curl_global_init(CURL_GLOBAL_ALL);
  init_redis();

  // Fetch `robots.txt` for each seed URL
  const char *seed_urls[] = {"https://archlinux.org/",
                             "https://www.google.com/", "https://github.com/"};
  for (int i = 0; i < NUM_THREADS; i++) {
    fetch_robots_txt(seed_urls[i]);
    push_url_to_queue(seed_urls[i]);
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, scrape, NULL) != 0) {
      fprintf(stderr, "Error creating thread\n");
      return EXIT_FAILURE;
    }
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  close_redis();
  curl_global_cleanup();
  return EXIT_SUCCESS;
}
