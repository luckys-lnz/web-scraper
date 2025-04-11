#include "data_store.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static PGconn *conn = NULL;

// SQL statements
static const char *CREATE_TABLES_SQL = 
    "CREATE TABLE IF NOT EXISTS pages ("
    "    url TEXT PRIMARY KEY,"
    "    title TEXT,"
    "    description TEXT,"
    "    keywords TEXT,"
    "    author TEXT,"
    "    crawl_time TIMESTAMP,"
    "    content_size BIGINT,"
    "    content_type TEXT,"
    "    status_code INTEGER,"
    "    response_time DOUBLE PRECISION"
    ");"
    "CREATE TABLE IF NOT EXISTS images ("
    "    id SERIAL PRIMARY KEY,"
    "    page_url TEXT REFERENCES pages(url),"
    "    src TEXT,"
    "    alt TEXT,"
    "    width INTEGER,"
    "    height INTEGER"
    ");"
    "CREATE TABLE IF NOT EXISTS links ("
    "    from_url TEXT REFERENCES pages(url),"
    "    to_url TEXT REFERENCES pages(url),"
    "    PRIMARY KEY (from_url, to_url)"
    ");";

int data_store_init(const char *conninfo) {
    LOG_INFO("Initializing database connection");
    
    conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        LOG_ERROR("Database connection failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        return -1;
    }

    // Create tables if they don't exist
    PGresult *res = PQexec(conn, CREATE_TABLES_SQL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        LOG_ERROR("Failed to create tables: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        return -1;
    }
    PQclear(res);

    return 0;
}

void data_store_cleanup() {
    LOG_INFO("Closing database connection");
    if (conn) {
        PQfinish(conn);
        conn = NULL;
    }
}

// Store page metadata in the database
bool store_page_metadata(const page_metadata_t *metadata) {
    if (!conn || !metadata) return false;

    const char *params[10];
    params[0] = metadata->url;
    params[1] = metadata->title;
    params[2] = metadata->description;
    params[3] = metadata->keywords;
    params[4] = metadata->author;
    params[5] = NULL; // Will be set to current timestamp
    params[6] = NULL; // Will be set to content_size
    params[7] = metadata->content_type;
    params[8] = NULL; // Will be set to status_code
    params[9] = NULL; // Will be set to response_time

    char content_size_str[32];
    char status_code_str[32];
    char response_time_str[32];
    char timestamp_str[32];

    // Convert metadata values to strings
    snprintf(content_size_str, sizeof(content_size_str), "%zu", metadata->content_size);
    snprintf(status_code_str, sizeof(status_code_str), "%d", metadata->status_code);
    snprintf(response_time_str, sizeof(response_time_str), "%.3f", metadata->response_time);
    snprintf(timestamp_str, sizeof(timestamp_str), "%ld", (long)metadata->crawl_time);

    params[5] = timestamp_str;
    params[6] = content_size_str;
    params[8] = status_code_str;
    params[9] = response_time_str;

    // Insert the page metadata into the database
    const char *sql = "INSERT INTO pages (url, title, description, keywords, author, "
                     "crawl_time, content_size, content_type, status_code, response_time) "
                     "VALUES ($1, $2, $3, $4, $5, to_timestamp($6), $7, $8, $9, $10) "
                     "ON CONFLICT (url) DO UPDATE SET "
                     "title = EXCLUDED.title, description = EXCLUDED.description, "
                     "keywords = EXCLUDED.keywords, author = EXCLUDED.author, "
                     "crawl_time = EXCLUDED.crawl_time, content_size = EXCLUDED.content_size, "
                     "content_type = EXCLUDED.content_type, status_code = EXCLUDED.status_code, "
                     "response_time = EXCLUDED.response_time";

    PGresult *res = PQexecParams(conn, sql, 10, NULL, params, NULL, NULL, 0);
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    if (!success) {
        LOG_ERROR("Failed to store page metadata: %s", PQerrorMessage(conn));
    }
    PQclear(res);
    return success;
}

// Store image data in the database
bool store_image_data(const image_data_t *image) {
    if (!conn || !image) return false;

    const char *params[5];
    params[0] = image->url;
    params[1] = image->src;
    params[2] = image->alt;
    params[3] = NULL; // Will be set to width
    params[4] = NULL; // Will be set to height

    char width_str[32];
    char height_str[32];
    snprintf(width_str, sizeof(width_str), "%d", image->width);
    snprintf(height_str, sizeof(height_str), "%d", image->height);
    params[3] = width_str;
    params[4] = height_str;

    const char *sql = "INSERT INTO images (page_url, src, alt, width, height) "
                     "VALUES ($1, $2, $3, $4, $5)";

    PGresult *res = PQexecParams(conn, sql, 5, NULL, params, NULL, NULL, 0);
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    if (!success) {
        LOG_ERROR("Failed to store image data: %s", PQerrorMessage(conn));
    }
    PQclear(res);
    return success;
}

// Store link relationship in the database
bool store_link_relationship(const char *from_url, const char *to_url) {
    if (!conn || !from_url || !to_url) return false;

    const char *params[2] = {from_url, to_url};
    const char *sql = "INSERT INTO links (from_url, to_url) VALUES ($1, $2) "
                     "ON CONFLICT DO NOTHING";

    PGresult *res = PQexecParams(conn, sql, 2, NULL, params, NULL, NULL, 0);
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    if (!success) {
        LOG_ERROR("Failed to store link relationship: %s", PQerrorMessage(conn));
    }
    PQclear(res);
    return success;
}

// Get page metadata from the database
bool get_page_metadata(const char *url, page_metadata_t *metadata) {
    if (!conn || !url || !metadata) return false;

    const char *params[1] = {url};
    const char *sql = "SELECT * FROM pages WHERE url = $1";

    PGresult *res = PQexecParams(conn, sql, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return false;
    }

    // Fill metadata structure
    metadata->url = strdup(PQgetvalue(res, 0, 0));
    metadata->title = strdup(PQgetvalue(res, 0, 1));
    metadata->description = strdup(PQgetvalue(res, 0, 2));
    metadata->keywords = strdup(PQgetvalue(res, 0, 3));
    metadata->author = strdup(PQgetvalue(res, 0, 4));
    metadata->crawl_time = atol(PQgetvalue(res, 0, 5));
    metadata->content_size = atol(PQgetvalue(res, 0, 6));
    metadata->content_type = strdup(PQgetvalue(res, 0, 7));
    metadata->status_code = atoi(PQgetvalue(res, 0, 8));
    metadata->response_time = atof(PQgetvalue(res, 0, 9));

    PQclear(res);
    return true;
}

// Get page images from the database
bool get_page_images(const char *url, image_data_t **images, int *count) {
    if (!conn || !url || !images || !count) return false;

    const char *params[1] = {url};
    const char *sql = "SELECT src, alt, width, height FROM images WHERE page_url = $1";

    PGresult *res = PQexecParams(conn, sql, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return false;
    }

    *count = PQntuples(res);
    *images = malloc(*count * sizeof(image_data_t));
    if (!*images) {
        PQclear(res);
        return false;
    }

    // Fill the image data structure
    for (int i = 0; i < *count; i++) {
        (*images)[i].url = strdup(url);
        (*images)[i].src = strdup(PQgetvalue(res, i, 0));
        (*images)[i].alt = strdup(PQgetvalue(res, i, 1));
        (*images)[i].width = atoi(PQgetvalue(res, i, 2));
        (*images)[i].height = atoi(PQgetvalue(res, i, 3));
    }

    PQclear(res);
    return true;
}

// Get page links from the database
bool get_page_links(const char *url, char ***links, int *count) {
    if (!conn || !url || !links || !count) return false;

    const char *params[1] = {url};
    const char *sql = "SELECT to_url FROM links WHERE from_url = $1";

    PGresult *res = PQexecParams(conn, sql, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return false;
    }

    *count = PQntuples(res);
    *links = malloc(*count * sizeof(char *));
    if (!*links) {
        PQclear(res);
        return false;
    }

    for (int i = 0; i < *count; i++) {
        (*links)[i] = strdup(PQgetvalue(res, i, 0));
    }

    PQclear(res);
    return true;
} 