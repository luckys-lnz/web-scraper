#include "robots_parser.h"
#include "fetch_url.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Extracts the domain from a URL.
 */
static char *extract_domain(const char *url) {
  char *domain_start = strstr(url, "://");
  if (!domain_start)
    return NULL;
  domain_start += 3;

  char *domain_end = strchr(domain_start, '/');
  size_t domain_len =
      domain_end ? (size_t)(domain_end - domain_start) : strlen(domain_start);

  char *domain = malloc(domain_len + 1);
  if (!domain)
    return NULL;

  strncpy(domain, domain_start, domain_len);
  domain[domain_len] = '\0';
  return domain;
}

/**
 * Fetches and prints `robots.txt` for a given domain.
 */
void fetch_robots_txt(const char *url) {
  char *domain = extract_domain(url);
  if (!domain)
    return;

  char robots_url[512];
  snprintf(robots_url, sizeof(robots_url), "https://%s/robots.txt", domain);
  free(domain);

  struct Memory chunk = {NULL, 0};
  fetch_url(robots_url, &chunk);

  if (chunk.response) {
    printf("\n=== Robots.txt for %s ===\n%s\n", url, chunk.response);
    free(chunk.response);
  }
}
