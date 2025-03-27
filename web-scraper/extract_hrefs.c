#include "scraper.h"

/**
 * Extracts and prints all hyperlinks (<a href="...") from the given HTML.
 *
 * @param html: Pointer to the HTML content.
 */
void extract_hrefs(const char *html) {
  regex_t regex;
  regmatch_t matches[2];

  // compile regex pattern
  if (regcomp(&regex, "<a href=\"([^\"]*)\">", REG_EXTENDED) != 0) {
    printf("Could not compile regex\n");
    return;
  }

  const char *cursor = html;

  while (regexec(&regex, cursor, 2, matches, 0) == 0) {
    int start = matches[1].rm_so;
    int end = matches[1].rm_eo;

    printf("Found URL: %.*s\n", end - start, cursor + start);

    // move cursor forward
    cursor += matches[0].rm_eo;
  }

  regfree(&regex);
}
