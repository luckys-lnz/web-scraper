/**
 * Source file for storing and checking robots.txt rules.
 * 
 * This file contains the implementation of the functions defined in the
 * robots_rules.h header file. It provides functionality to store and check
 * robots.txt rules in a cache.
 */
#include "robots_rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RobotsRules robots_cache[128];
static int robots_cache_count = 0;

void store_robots_rules(const char *domain, const char *content) {
  if (!domain || !content) return;

  RobotsRules *rules = &robots_cache[robots_cache_count++];
  strncpy(rules->domain, domain, sizeof(rules->domain));

  char *copy = strdup(content);
  char *line = strtok(copy, "\n");
  while (line && rules->rule_count < MAX_RULES) {
    if (strncmp(line, "Disallow:", 9) == 0) {
      char *path = line + 9;
      while (*path == ' ' || *path == '\t') path++;
      rules->disallowed[rules->rule_count++] = strdup(path);
    }
    line = strtok(NULL, "\n");
  }
  free(copy);
}

int is_path_allowed(const char *domain, const char *path) {
  for (int i = 0; i < robots_cache_count; ++i) {
    if (strcmp(robots_cache[i].domain, domain) == 0) {
      RobotsRules *rules = &robots_cache[i];
      for (int j = 0; j < rules->rule_count; ++j) {
        if (strncmp(path, rules->disallowed[j], strlen(rules->disallowed[j])) == 0) {
          return 0; // Disallowed
        }
      }
      break;
    }
  }
  return 1; // Allowed by default
}
