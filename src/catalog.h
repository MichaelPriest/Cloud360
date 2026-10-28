#ifndef CLOUD360_CATALOG_H
#define CLOUD360_CATALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Application-owned catalogue schema. This is NOT a Microsoft/Game Pass API.
 * All incoming JSON is untrusted, including local files. */
enum {
    CLOUD360_CATALOG_JSON_MAX = 16384,
    CLOUD360_CATALOG_MAX_GAMES = 36,
    CLOUD360_CATALOG_ID_BYTES = 40,
    CLOUD360_CATALOG_TITLE_BYTES = 80,
    CLOUD360_CATALOG_CATEGORY_BYTES = 40,
    CLOUD360_CATALOG_DESCRIPTION_BYTES = 96
};

typedef struct {
    char id[CLOUD360_CATALOG_ID_BYTES];
    char title[CLOUD360_CATALOG_TITLE_BYTES];
    char category[CLOUD360_CATALOG_CATEGORY_BYTES];
    char description[CLOUD360_CATALOG_DESCRIPTION_BYTES];
} Cloud360Game;

typedef struct {
    Cloud360Game games[CLOUD360_CATALOG_MAX_GAMES];
    uint32_t count;
    bool loaded;
} Cloud360Catalog;

void cloud360_catalog_init(Cloud360Catalog *catalog);

/* Expected JSON: {"games":[{"id":"...","title":"...",
 * "category":"...","description":"..."}, ...]}
 *
 * Strictly bounded recursive parser, no heap use. Unknown JSON fields are
 * validated and skipped. On failure, the previous catalogue stays intact. */
bool cloud360_catalog_parse(Cloud360Catalog *catalog,
                            const char *json, size_t length);

/* Read a local JSON file with hard byte cap. Missing/invalid file does NOT
 * populate a demo catalogue or impersonate Xbox Cloud Gaming. */
bool cloud360_catalog_load_file(Cloud360Catalog *catalog, const char *path);

/* Case-insensitive ASCII search; matching accented UTF-8 is byte-based.
 * Returns number of matches; stores up to capacity matching game indices. */
size_t cloud360_catalog_search(const Cloud360Catalog *catalog,
                               const char *query, uint32_t *indices,
                               size_t capacity);

#endif
