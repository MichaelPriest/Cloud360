#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/settings.h"

static const char *const path = "/tmp/cloud360-settings-test.cfg";

static void test_defaults_and_roundtrip(void)
{
    Cloud360Settings s;
    cloud360_settings_defaults(&s);
    assert(s.animations && s.ambient_effects && !s.high_contrast && s.show_hints);
    s.animations = false;
    s.ambient_effects = false;
    s.high_contrast = true;
    s.show_hints = false;
    assert(cloud360_settings_save(&s, path));
    cloud360_settings_defaults(&s);
    assert(cloud360_settings_load(&s, path));
    assert(!s.animations && !s.ambient_effects && s.high_contrast && !s.show_hints);
}

static void test_corrupt_config_keeps_previous_settings(void)
{
    Cloud360Settings s;
    cloud360_settings_defaults(&s);
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fputs("CLOUD360_SETTINGS_V1 4294967295 0\n", f) >= 0);
    assert(fclose(f) == 0);
    assert(!cloud360_settings_load(&s, path));
    assert(s.animations && s.ambient_effects && !s.high_contrast && s.show_hints);
    assert(remove(path) == 0);
    assert(!cloud360_settings_load(&s, path));
}

int main(void)
{
    test_defaults_and_roundtrip();
    test_corrupt_config_keeps_previous_settings();
    puts("Cloud360 persistence: PASS");
    return 0;
}
