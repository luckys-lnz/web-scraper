/**
 * Header file for storing and checking robots.txt rules.
 */
#ifndef ROBOTS_RULES_H
#define ROBOTS_RULES_H

#define MAX_RULES 128

typedef struct {
  char domain[256];
  char *disallowed[MAX_RULES];
  int rule_count;
} RobotsRules;

void store_robots_rules(const char *domain, const char *content);
int is_path_allowed(const char *domain, const char *path);

#endif
