#include "redis_helper.h"
#include "scraper.h"
#include <libxml/HTMLparser.h>
#include <libxml/uri.h>
#include <libxml/xmlmemory.h>
#include <libxml/xpath.h>
#include <regex.h>
#include "logger.h"
#include "robots_parser.h"  // For redis_ctx declaration

// Redis context (declared in robots_parser.h)
extern redisContext *redis_ctx;

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

  // use libxml's URI functions to resolve relative URLs
  xmlChar *escped_base = xmlURIEscapeStr((const xmlChar *)base_url, NULL);
  xmlChar *escped_href = xmlURIEscapeStr((const xmlChar *)href, NULL);

  xmlChar *absolute_uri = xmlBuildURI(escped_base, escped_href);
  xmlFree(escped_base);
  xmlFree(escped_href);

  // Check if the URI is valid
  if (!absolute_uri)
    return NULL;

  // convert to stirng
  char *result = strdup((const char *)absolute_uri);
  xmlFree(absolute_uri);

  // Remove fragment identifiers (`#...`)
  char *fragment = strchr(result, '#');
  if (fragment)
    *fragment = '\0'; // Truncate at `#`

  // Check if the URL is valid
  size_t len = strlen(result);
  if (len > 1 && result[len - 1] == '/') {
    result[len - 1] = '\0'; // Remove trailing slash
  }

  return result;
}

/**
 * Extracts and processes all hyperlinks (<a href="...") from the given HTML.
 *
 * @param html Pointer to the HTML content.
 * @param base_url The base URL of the page.
 */
void extract_hrefs(const char *html, const char *base_url) {
  if (!html || !base_url) {
    LOG_ERROR("Invalid parameters to extract_hrefs");
    return;
  }

  xmlDocPtr doc = htmlReadMemory(html, strlen(html), NULL, NULL,
                                 HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                                     HTML_PARSE_NOWARNING);
  if (!doc) {
    LOG_ERROR("Failed to parse HTML document");
    return;
  }

  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  if (!context) {
    LOG_ERROR("Failed to create XPath context");
    xmlFreeDoc(doc);
    return;
  }

  xmlXPathObjectPtr result = xmlXPathEvalExpression((xmlChar *)"//a[@href]", context);
  if (!result) {
    LOG_ERROR("Failed to evaluate XPath expression");
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    return;
  }

  if (!result->nodesetval || result->nodesetval->nodeNr == 0) {
    xmlXPathFreeObject(result);
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    return;
  }

  // Check Redis connection before processing URLs
  if (!redis_ctx || redis_ctx->err) {
    LOG_ERROR("Redis connection not available for URL processing");
    xmlXPathFreeObject(result);
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    return;
  }

  for (int i = 0; i < result->nodesetval->nodeNr; i++) {
    xmlNodePtr node = result->nodesetval->nodeTab[i];
    if (!node) continue;

    xmlChar *href = xmlGetProp(node, (xmlChar *)"href");
    if (!href) continue;

    char *normalized_url = normalize_url(base_url, (char *)href);
    xmlFree(href);

    if (normalized_url) {
      // Check if URL is already visited
      int visited = is_visited(normalized_url);
      if (!visited) {
        // Add to queue if not visited
        push_url_to_queue(normalized_url, 1);
        LOG_INFO("Discovered: %s", normalized_url);
      }
      free(normalized_url);
    }
  }

  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);
  xmlFreeDoc(doc);
}
