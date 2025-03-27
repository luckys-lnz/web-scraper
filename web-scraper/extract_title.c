#include "scraper.h"

/**
 * Extracts and prints the content inside the <title> tag from the given HTML.
 *
 * @param html: Pointer to the HTML content.
 */
void extract_title(const char *html) {
  xmlDocPtr doc = htmlReadMemory(html, strlen(html), NULL, NULL,
                                 HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                                     HTML_PARSE_NOWARNING);
  if (!doc) {
    fprintf(stderr, "Failed to parse HTML\n");
    return;
  }

  // xmlNodePtr root = xmlDocGetRootElement(doc);
  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  xmlXPathObjectPtr result =
      xmlXPathEvalExpression((xmlChar *)"//title", context);

  if (result && result->nodesetval->nodeNr > 0) {
    xmlNodePtr node = result->nodesetval->nodeTab[0];
    printf("Title: %s\n", xmlNodeGetContent(node));
  } else {
    printf("No <title> found.\n");
  }

  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);
  xmlFreeDoc(doc);
}
