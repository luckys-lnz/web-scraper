#include "scraper.h"

/**
 * Extracts and prints all hyperlinks (<a href="...">) from the given HTML.
 *
 * @param html: Pointer to the HTML content.
 */
void extract_hrefs(const char *html) {
  if (!html)
    return;

  xmlDocPtr doc = htmlReadMemory(html, strlen(html), NULL, NULL,
                                 HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                                     HTML_PARSE_NOWARNING);
  if (!doc) {
    fprintf(stderr, "Failed to parse HTML document\n");
    return;
  }

  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  if (!context) {
    fprintf(stderr, "Failed to create XPath context\n");
    xmlFreeDoc(doc);
    return;
  }

  // Find all <a> elements
  xmlXPathObjectPtr result =
      xmlXPathEvalExpression((xmlChar *)"//a[@href]", context);
  if (!result) {
    fprintf(stderr, "XPath evaluation failed\n");
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    return;
  }

  if (!result->nodesetval || result->nodesetval->nodeNr == 0) {
    printf("No hyperlinks found.\n");
  } else {
    for (int i = 0; i < result->nodesetval->nodeNr; i++) {
      xmlNodePtr node = result->nodesetval->nodeTab[i];
      xmlChar *href = xmlGetProp(node, (xmlChar *)"href");

      if (href) {
        printf("Found URL: %s\n", href);
        xmlFree(href);
      }
    }
  }

  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);
  xmlFreeDoc(doc);
}

