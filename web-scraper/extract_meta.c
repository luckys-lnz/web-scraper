#include "scraper.h"

/**
 * Extracts and prints all <meta> tags from the given HTML.
 *
 * @param html: Pointer to the HTML content.
 */
void extract_meta(const char *html) {
  xmlDocPtr doc = htmlReadMemory(html, strlen(html), NULL, NULL,
                                 HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                                     HTML_PARSE_NOWARNING);
  if (!doc)
    return;

  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  xmlXPathObjectPtr result =
      xmlXPathEvalExpression((xmlChar *)"//meta", context);

  if (result && result->nodesetval->nodeNr > 0) {
    for (int i = 0; i < result->nodesetval->nodeNr; i++) {
      xmlNodePtr node = result->nodesetval->nodeTab[i];
      xmlChar *name = xmlGetProp(node, (xmlChar *)"name");
      xmlChar *content = xmlGetProp(node, (xmlChar *)"content");

      if (name && content) {
        printf("Meta: name=\"%s\", content=\"%s\"\n", name, content);
      }

      xmlFree(name);
      xmlFree(content);
    }
  }

  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);
  xmlFreeDoc(doc);
}
