#include "scraper.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 3

void *scrape(void *arg) {
  const char *url = (const char *)arg;
  struct Memory chunk = {NULL, 0};

  fetch_url(url, &chunk);

  if (chunk.response) {
    extract_title(chunk.response);
    extract_meta(chunk.response);
    extract_hrefs(chunk.response);
    free(chunk.response);
  }

  pthread_exit(NULL);
}

int main() {
  pthread_t threads[NUM_THREADS];
  const char *urls[] = {"https://archlinux.org/", "https://www.google.com/",
                        "https://github.com/"};

  for (int i = 0; i < NUM_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, scrape, (void *)urls[i]) != 0) {
      fprintf(stderr, "Error creating thread\n");
      return 1;
    }
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}
