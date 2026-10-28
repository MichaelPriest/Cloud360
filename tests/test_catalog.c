#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/catalog.h"

static Cloud360Catalog catalog;

static void test_valid_and_search(void)
{
    static const char input[] =
        "{\"version\":1,\"ignored\":{\"nested\":[true,null,{\"v\":-2.5e+2}]},"
        "\"games\":["
        "{\"id\":\"a1\",\"title\":\"Corrida Azul\","
        "\"category\":\"Corrida\",\"description\":\"Piloto \\u00e1gil\"},"
        "{\"id\":\"a2\",\"title\":\"Aventura do C\\u00e9u\","
        "\"category\":\"Aventura\",\"extra\":[1,2,3]},"
        "{\"id\":\"a3\",\"title\":\"Novo \\uD83C\\uDFAE Jogo\","
        "\"category\":\"A\\u00e7\\u00e3o\"}]}";
    assert(cloud360_catalog_parse(&catalog, input, sizeof input - 1));
    assert(catalog.loaded && catalog.count == 3);
    assert(strcmp(catalog.games[1].title, "Aventura do Céu") == 0);
    assert(strcmp(catalog.games[0].description, "Piloto ágil") == 0);
    assert(strcmp(catalog.games[1].description, "") == 0);
    assert(strcmp(catalog.games[0].id, "a1") == 0);

    uint32_t found[3] = { 999, 999, 999 };
    assert(cloud360_catalog_search(&catalog, "cOrRiDa", found, 3) == 1);
    assert(found[0] == 0);
    assert(cloud360_catalog_search(&catalog, "AVENTURA", found, 3) == 1);
    assert(found[0] == 1);
    assert(cloud360_catalog_search(&catalog, "", found, 2) == 3);
    assert(found[0] == 0 && found[1] == 1);
    assert(cloud360_catalog_search(&catalog, "not here", found, 3) == 0);
    assert(cloud360_catalog_search(&catalog, "", NULL, 3) == 0);
}

static void expect_reject(const char *json)
{
    const uint32_t previous = catalog.count;
    assert(!cloud360_catalog_parse(&catalog, json, strlen(json)));
    assert(catalog.loaded && catalog.count == previous);
    assert(strcmp(catalog.games[0].id, "a1") == 0);
}

static void test_bad_json(void)
{
    expect_reject("{\"games\":[{\"id\":\"a1\",\"title\":\"A\"},"
                  "{\"id\":\"a1\",\"title\":\"B\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"x\",\"id\":\"b\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"x\","
                  "\"category\":\"z\",\"category\":\"y\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"bad\\uD800\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"bad\\uDC00\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"bad\\uD800\\u0041\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"bad\\q\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"bad\\ntext\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"bad\\u0000\"}]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"ok\"}],\"games\":[]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"ok\"}],\"broken\":[1,]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"ok\"}],\"broken\":01}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"ok\"}],\"broken\":1.}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"ok\"}],\"broken\":1e}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"ok\"}]} garbage");
    expect_reject("{\"games\":{} }");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"ok\"},]}");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"ok\"}]");
    expect_reject("{}");
    expect_reject("[1,2]");
    expect_reject("{\"games\":[{\"id\":\"a\",\"title\":\"X\","
                  "\"description\":\"This text is too long "
                  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ\"}]}");

    char huge[CLOUD360_CATALOG_JSON_MAX + 2] = { 0 };
    memset(huge, 'X', sizeof huge);
    assert(!cloud360_catalog_parse(&catalog, huge, sizeof huge));
}

static void test_capacity_and_files(void)
{
    char json[12000] = "{\"games\":[";
    size_t n = strlen(json);
    for (int i = 0; i < CLOUD360_CATALOG_MAX_GAMES; ++i) {
        int written = snprintf(json + n, sizeof json - n,
                               "%s{\"id\":\"id%d\",\"title\":\"Game %d\"}",
                               i ? "," : "", i, i);
        assert(written > 0);
        n += (size_t)written;
    }
    memcpy(json + n, "]}", 3);
    n += 2;
    assert(cloud360_catalog_parse(&catalog, json, n));
    assert(catalog.count == CLOUD360_CATALOG_MAX_GAMES);

    /* Restoring the fixture for the following failure-preservation checks. */
    FILE *file = fopen("/tmp/cloud360-catalog-test.json", "wb");
    assert(file);
    assert(fwrite(json, 1, n, file) == n);
    assert(fclose(file) == 0);
    cloud360_catalog_init(&catalog);
    assert(cloud360_catalog_load_file(&catalog,
                                      "/tmp/cloud360-catalog-test.json"));
    assert(catalog.count == CLOUD360_CATALOG_MAX_GAMES);
    assert(remove("/tmp/cloud360-catalog-test.json") == 0);
    assert(!cloud360_catalog_load_file(&catalog,
                                       "/tmp/cloud360-catalog-test.json"));
    assert(catalog.count == CLOUD360_CATALOG_MAX_GAMES);

    /* Over the hard byte cap (even with valid JSON at the start). */
    char over[CLOUD360_CATALOG_JSON_MAX + 1];
    memset(over, ' ', sizeof over);
    memcpy(over, "{\"games\":[]}", sizeof "{\"games\":[]}" - 1);
    file = fopen("/tmp/cloud360-catalog-test.json", "wb");
    assert(file);
    assert(fwrite(over, 1, sizeof over, file) == sizeof over);
    assert(fclose(file) == 0);
    assert(!cloud360_catalog_load_file(&catalog,
                                       "/tmp/cloud360-catalog-test.json"));
    assert(remove("/tmp/cloud360-catalog-test.json") == 0);
}

int main(void)
{
    cloud360_catalog_init(&catalog);
    test_valid_and_search();
    test_bad_json();
    test_capacity_and_files();
    puts("Cloud360 bounded catalog JSON parser, search and file: PASS");
    return 0;
}
