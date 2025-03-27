#include "scraper.h"

/**
 * Extracts and prints the content inside the <title> tag from the given HTML.
 *
 * @param html: Pointer to the HTML content.
 */
void extract_title(const char *html) {
  if (!html)
    return;

  xmlDocPtr doc = htmlReadMemory(html, strlen(html), NULL, NULL,
                                 HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                                     HTML_PARSE_NOWARNING);
  if (!doc) {
    fprintf(stderr, "Failed to parse HTML\n");
    return;
  }

  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  if (!context) {
    fprintf(stderr, "Failed to create XPath context\n");
    xmlFreeDoc(doc);
    return;
  }

  xmlXPathObjectPtr result =
      xmlXPathEvalExpression((xmlChar *)"//title", context);
  if (!result) {
    fprintf(stderr, "XPath evaluation failed\n");
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    return;
  }

  if (result->nodesetval && result->nodesetval->nodeNr > 0) {
    xmlNodePtr node = result->nodesetval->nodeTab[0];
    xmlChar *title = xmlNodeGetContent(node);

    if (title) {
      printf("Title: %s\n", title);
      xmlFree(title);
    }
  } else {
    printf("No <title> found.\n");
  }

  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);
  xmlFreeDoc(doc);
}

