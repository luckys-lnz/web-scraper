#include "scraper.h"

/**
 * Extracts and prints all <meta> tags (both name/content and property/content).
 *
 * @param html: Pointer to the HTML content.
 */
void extract_meta(const char *html) {
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

  xmlXPathObjectPtr result =
      xmlXPathEvalExpression((xmlChar *)"//meta", context);
  if (!result) {
    fprintf(stderr, "XPath evaluation failed\n");
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    return;
  }

  if (!result->nodesetval || result->nodesetval->nodeNr == 0) {
    fprintf(stderr, "No <meta> tags found\n");
  } else {
    for (int i = 0; i < result->nodesetval->nodeNr; i++) {
      xmlNodePtr node = result->nodesetval->nodeTab[i];
      xmlChar *name = xmlGetProp(node, (xmlChar *)"name");
      xmlChar *content = xmlGetProp(node, (xmlChar *)"content");
      xmlChar *property =
          xmlGetProp(node, (xmlChar *)"property"); // For Open Graph meta tags

      if ((name && content) || (property && content)) {
        printf("Meta: %s=\"%s\", content=\"%s\"\n", name ? "name" : "property",
               name ? name : property, content);
      }

      xmlFree(name);
      xmlFree(property);
      xmlFree(content);
    }
  }

  xmlXPathFreeObject(result);
  xmlXPathFreeContext(context);
  xmlFreeDoc(doc);
}

