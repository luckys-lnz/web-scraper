#ifndef WRITE_CALLBACK_H
#define WRITE_CALLBACK_H

#include "scraper.h"
#include <stddef.h>

/**
 * Callback function for CURL to write received data.
 */
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);

#endif // WRITE_CALLBACK_H
