#include "catalog.h"

#include <stdio.h>
#include <string.h>

enum { MAX_JSON_DEPTH = 12, MAX_KEY_BYTES = 48 };
typedef struct {
    const char *text;
    size_t len;
    size_t pos;
} Json;

static void spaces(Json *p)
{
    while (p->pos < p->len) {
        const char c = p->text[p->pos];
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t')
            break;
        ++p->pos;
    }
}

static bool consume(Json *p, char ch)
{
    spaces(p);
    if (p->pos < p->len && p->text[p->pos] == ch) {
        ++p->pos;
        return true;
    }
    return false;
}

static bool push(char *out, size_t capacity, size_t *used, unsigned char byte)
{
    if (!out)
        return true;
    if (*used >= capacity - 1)
        return false;
    out[(*used)++] = (char)byte;
    return true;
}

static bool hex4(Json *p, uint32_t *cp)
{
    if (p->len - p->pos < 4)
        return false;
    uint32_t number = 0;
    for (int i = 0; i < 4; ++i) {
        const unsigned char c = (unsigned char)p->text[p->pos++];
        unsigned nibble;
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else return false;
        number = (number << 4) | nibble;
    }
    *cp = number;
    return true;
}

static bool utf8(char *out, size_t cap, size_t *used, uint32_t cp)
{
    if (cp <= 0x7F)
        return push(out, cap, used, (unsigned char)cp);
    if (cp <= 0x7FF)
        return push(out, cap, used, (unsigned char)(0xC0 | (cp >> 6))) &&
               push(out, cap, used, (unsigned char)(0x80 | (cp & 63)));
    if (cp <= 0xFFFF)
        return push(out, cap, used, (unsigned char)(0xE0 | (cp >> 12))) &&
               push(out, cap, used, (unsigned char)(0x80 | ((cp >> 6) & 63))) &&
               push(out, cap, used, (unsigned char)(0x80 | (cp & 63)));
    if (cp > 0x10FFFF) return false;
    return push(out, cap, used, (unsigned char)(0xF0 | (cp >> 18))) &&
           push(out, cap, used, (unsigned char)(0x80 | ((cp >> 12) & 63))) &&
           push(out, cap, used, (unsigned char)(0x80 | ((cp >> 6) & 63))) &&
           push(out, cap, used, (unsigned char)(0x80 | (cp & 63)));
}

/* Parses a quoted JSON string, including \uXXXX surrogate pairs.
 * Control characters are forbidden in materialized field values. */
static bool string(Json *p, char *out, size_t cap)
{
    spaces(p);
    if (p->pos >= p->len || p->text[p->pos++] != '"')
        return false;
    size_t used = 0;
    while (p->pos < p->len) {
        const unsigned char c = (unsigned char)p->text[p->pos++];
        if (c == '"') {
            if (out) out[used] = '\0';
            return true;
        }
        if (c < 0x20) return false;
        if (c != '\\') {
            if (!push(out, cap, &used, c)) return false;
            continue;
        }
        if (p->pos >= p->len) return false;
        const char escape = p->text[p->pos++];
        uint32_t cp = 0;
        switch (escape) {
            case '"': cp = '"'; break;
            case '\\': cp = '\\'; break;
            case '/': cp = '/'; break;
            case 'b': cp = '\b'; break;
            case 'f': cp = '\f'; break;
            case 'n': cp = '\n'; break;
            case 'r': cp = '\r'; break;
            case 't': cp = '\t'; break;
            case 'u': {
                if (!hex4(p, &cp)) return false;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    if (p->len - p->pos < 6 ||
                        p->text[p->pos++] != '\\' ||
                        p->text[p->pos++] != 'u')
                        return false;
                    uint32_t low = 0;
                    if (!hex4(p, &low) || low < 0xDC00 || low > 0xDFFF)
                        return false;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) return false;
                break;
            }
            default: return false;
        }
        if (out && cp < 0x20) return false;
        if (!utf8(out, cap, &used, cp)) return false;
    }
    return false;
}

static bool number(Json *p)
{
    const size_t start = p->pos;
    if (p->pos < p->len && p->text[p->pos] == '-') ++p->pos;
    if (p->pos >= p->len) return false;
    if (p->text[p->pos] == '0') ++p->pos;
    else {
        if (p->text[p->pos] < '1' || p->text[p->pos] > '9')
            return false;
        do { ++p->pos; }
        while (p->pos < p->len &&
               p->text[p->pos] >= '0' && p->text[p->pos] <= '9');
    }
    if (p->pos < p->len && p->text[p->pos] == '.') {
        ++p->pos;
        const size_t digits = p->pos;
        while (p->pos < p->len && p->text[p->pos] >= '0' &&
               p->text[p->pos] <= '9') ++p->pos;
        if (digits == p->pos) return false;
    }
    if (p->pos < p->len &&
        (p->text[p->pos] == 'e' || p->text[p->pos] == 'E')) {
        ++p->pos;
        if (p->pos < p->len &&
            (p->text[p->pos] == '+' || p->text[p->pos] == '-')) ++p->pos;
        const size_t digits = p->pos;
        while (p->pos < p->len && p->text[p->pos] >= '0' &&
               p->text[p->pos] <= '9') ++p->pos;
        if (digits == p->pos) return false;
    }
    return p->pos > start;
}

static bool literal(Json *p, const char *token)
{
    const size_t len = strlen(token);
    if (p->len - p->pos < len || memcmp(p->text + p->pos, token, len))
        return false;
    p->pos += len;
    return true;
}

static bool skip(Json *p, unsigned depth)
{
    if (depth > MAX_JSON_DEPTH) return false;
    spaces(p);
    if (p->pos >= p->len) return false;
    const char type = p->text[p->pos];
    if (type == '"') return string(p, NULL, 0);
    if (type == '[') {
        ++p->pos;
        if (consume(p, ']')) return true;
        do {
            if (!skip(p, depth + 1)) return false;
            if (consume(p, ']')) return true;
        } while (consume(p, ','));
        return false;
    }
    if (type == '{') {
        ++p->pos;
        if (consume(p, '}')) return true;
        do {
            if (!string(p, NULL, 0) || !consume(p, ':') ||
                !skip(p, depth + 1)) return false;
            if (consume(p, '}')) return true;
        } while (consume(p, ','));
        return false;
    }
    if (type == 't') return literal(p, "true");
    if (type == 'f') return literal(p, "false");
    if (type == 'n') return literal(p, "null");
    return number(p);
}

static bool game(Json *p, Cloud360Game *out)
{
    if (!consume(p, '{')) return false;
    bool id = false, title = false, category = false, description = false;
    if (consume(p, '}')) return false;
    do {
        char key[MAX_KEY_BYTES] = { 0 };
        if (!string(p, key, sizeof key) || !consume(p, ':')) return false;
        if (strcmp(key, "id") == 0) {
            if (id || !string(p, out->id, sizeof out->id)) return false;
            id = true;
        } else if (strcmp(key, "title") == 0) {
            if (title || !string(p, out->title, sizeof out->title)) return false;
            title = true;
        } else if (strcmp(key, "category") == 0) {
            if (category || !string(p, out->category, sizeof out->category))
                return false;
            category = true;
        } else if (strcmp(key, "description") == 0) {
            if (description || !string(p, out->description, sizeof out->description))
                return false;
            description = true;
        } else if (!skip(p, 1)) return false;

        if (consume(p, '}')) break;
        if (!consume(p, ',')) return false;
    } while (true);
    if (!id || !title || !out->id[0] || !out->title[0]) return false;
    if (!category) memcpy(out->category, "SEM CATEGORIA", sizeof "SEM CATEGORIA");
    return true;
}

static bool games(Json *p, Cloud360Catalog *out)
{
    if (!consume(p, '[')) return false;
    if (consume(p, ']')) return true;
    do {
        if (out->count >= CLOUD360_CATALOG_MAX_GAMES) return false;
        Cloud360Game *entry = &out->games[out->count];
        if (!game(p, entry)) return false;
        for (uint32_t i = 0; i < out->count; ++i)
            if (strcmp(out->games[i].id, entry->id) == 0) return false;
        ++out->count;
        if (consume(p, ']')) return true;
        if (!consume(p, ',')) return false;
    } while (true);
}

void cloud360_catalog_init(Cloud360Catalog *out)
{
    if (out) memset(out, 0, sizeof *out);
}

bool cloud360_catalog_parse(Cloud360Catalog *out, const char *json, size_t length)
{
    if (!out || !json || !length || length > CLOUD360_CATALOG_JSON_MAX)
        return false;

    /* Static scratch: no large stack frame on the console's render thread.
     * The caller serializes writes (rendering only receives a finished copy). */
    static Cloud360Catalog scratch;
    cloud360_catalog_init(&scratch);
    Json p = { json, length, 0 };
    if (!consume(&p, '{')) return false;
    bool found_games = false;
    if (!consume(&p, '}')) {
        do {
            char key[MAX_KEY_BYTES] = { 0 };
            if (!string(&p, key, sizeof key) || !consume(&p, ':'))
                return false;
            if (strcmp(key, "games") == 0) {
                if (found_games || !games(&p, &scratch)) return false;
                found_games = true;
            } else if (!skip(&p, 1)) return false;
            if (consume(&p, '}')) break;
            if (!consume(&p, ',')) return false;
        } while (true);
    }
    spaces(&p);
    if (!found_games || p.pos != p.len) return false;
    scratch.loaded = true;
    *out = scratch;
    return true;
}

bool cloud360_catalog_load_file(Cloud360Catalog *out, const char *path)
{
    if (!out || !path) return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;

    static char buffer[CLOUD360_CATALOG_JSON_MAX + 1];
    const size_t n = fread(buffer, 1, sizeof buffer, file);
    const bool fine = !ferror(file) && n > 0 && n <= CLOUD360_CATALOG_JSON_MAX;
    const bool close_ok = fclose(file) == 0;
    return fine && close_ok && cloud360_catalog_parse(out, buffer, n);
}

static unsigned char lowercase(unsigned char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? (unsigned char)(ch + ('a' - 'A')) : ch;
}

static bool contains(const char *text, const char *query)
{
    if (!query || !*query) return true;
    for (size_t i = 0; text[i]; ++i) {
        size_t j = 0;
        while (query[j] && text[i + j] &&
               lowercase((unsigned char)text[i + j]) ==
                   lowercase((unsigned char)query[j])) ++j;
        if (!query[j]) return true;
    }
    return false;
}

size_t cloud360_catalog_search(const Cloud360Catalog *catalog,
                               const char *query, uint32_t *indices,
                               size_t capacity)
{
    if (!catalog || !catalog->loaded || (!indices && capacity))
        return 0;
    size_t matches = 0;
    for (uint32_t i = 0; i < catalog->count; ++i) {
        if (!contains(catalog->games[i].title, query) &&
            !contains(catalog->games[i].category, query)) continue;
        if (matches < capacity) indices[matches] = i;
        ++matches;
    }
    return matches;
}
