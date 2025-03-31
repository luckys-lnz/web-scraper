#include "redis_helper.h"
#include "scraper.h"
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Normalizes and sanitizes a URL:
 * - Converts relative paths to absolute using the base URL.
 * - Skips JavaScript, mailto, and non-HTTP links.
 * - Removes URL fragments (`#...`).
 *
 * @param base_url The base page URL.
 * @param href The extracted href value.
 * @return A dynamically allocated absolute URL (caller must free) or NULL if
 * invalid.
 */
char *normalize_url(const char *base_url, const char *href) {
  if (!href || strlen(href) == 0)
    return NULL;

  // Ignore JavaScript and mailto links
  if (strncmp(href, "javascript:", 11) == 0 ||
      strncmp(href, "mailto:", 7) == 0) {
    return NULL;
  }

  // Remove fragment identifiers (`#...`)
  char *hash = strchr(href, '#');
  if (hash) {
    *hash = '\0'; // Truncate at `#`
  }

  // If already absolute (http/https), return a copy
  if (strncmp(href, "http://", 7) == 0 || strncmp(href, "https://", 8) == 0) {
    return strdup(href);
  }

  // Ensure base_url ends with '/'
  size_t base_len = strlen(base_url);
  size_t href_len = strlen(href);
  int needs_slash = (base_url[base_len - 1] != '/') && (href[0] != '/');

  char *absolute_url = malloc(base_len + href_len + (needs_slash ? 2 : 1));
  if (!absolute_url) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }

  strcpy(absolute_url, base_url);
  if (needs_slash)
    strcat(absolute_url, "/");
  strcat(absolute_url, href);

  return absolute_url;
}

/**
 * Extracts and processes all hyperlinks (<a href="...") from the given HTML.
 *
 * @param html Pointer to the HTML content.
 * @param base_url The base URL of the page.
 */
void extract_hrefs(const char *html, const char *base_url) {
  xmlDocPtr doc = htmlReadMemory(html, strlen(html), NULL, NULL,
                                 HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                                     HTML_PARSE_NOWARNING);
  if (!doc)
    return;

  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  xmlXPathObjectPtr result =
      xmlXPathEvalExpression((xmlChar *)"//a[@href]", context);

  if (!result || !result->nodesetval || result->nodesetval->nodeNr == 0) {
    xmlXPathFreeObject(result);
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    return;
  }

  for (int i = 0; i < result->nodesetval->nodeNr; i++) {
    xmlNodePtr node = result->nodesetval->nodeTab[i];
    xmlChar *href = xmlGetProp(node, (xmlChar *)"href");

    if (href) {
      char *normalized_url = normalize_url(base_url, (char *)href);

      if (normalized_url) {
        if (!is_visited(normalized_url)) { // Only add if not visited
          push_url_to_queue(normalized_url);
          printf("Discovered: %s\n", normalized_url);
        }
        free(normalized_url);
      }

      xmlFree(href);
    }
  }

  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);
  xmlFreeDoc(doc);
}
