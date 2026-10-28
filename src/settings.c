#include "settings.h"

#include <stdio.h>
#include <string.h>

enum {
    FLAG_ANIMATIONS = 1,
    FLAG_AMBIENT = 2,
    FLAG_CONTRAST = 4,
    FLAG_HINTS = 8,
    ALL_FLAGS = 15
};
#define SETTINGS_MAGIC "CLOUD360_SETTINGS_V1"
#define SETTINGS_CHECK 0xC360A55Au

void cloud360_settings_defaults(Cloud360Settings *settings)
{
    settings->animations = true;
    settings->ambient_effects = true;
    settings->high_contrast = false;
    settings->show_hints = true;
}

static uint32_t encode(const Cloud360Settings *settings)
{
    uint32_t flags = 0;
    if (settings->animations) flags |= FLAG_ANIMATIONS;
    if (settings->ambient_effects) flags |= FLAG_AMBIENT;
    if (settings->high_contrast) flags |= FLAG_CONTRAST;
    if (settings->show_hints) flags |= FLAG_HINTS;
    return flags;
}

bool cloud360_settings_load(Cloud360Settings *settings, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;

    char marker[32] = { 0 };
    unsigned int flags = 0;
    unsigned int check = 0;
    const int count = fscanf(file, "%31s %u %u", marker, &flags, &check);
    const int close_status = fclose(file);

    if (close_status != 0 || count != 3 ||
        strcmp(marker, SETTINGS_MAGIC) != 0 ||
        (flags & ~((unsigned int)ALL_FLAGS)) != 0 ||
        check != (flags ^ SETTINGS_CHECK))
        return false;

    settings->animations = (flags & FLAG_ANIMATIONS) != 0;
    settings->ambient_effects = (flags & FLAG_AMBIENT) != 0;
    settings->high_contrast = (flags & FLAG_CONTRAST) != 0;
    settings->show_hints = (flags & FLAG_HINTS) != 0;
    return true;
}

bool cloud360_settings_save(const Cloud360Settings *settings, const char *path)
{
    FILE *file = fopen(path, "wb");
    if (!file)
        return false;

    const uint32_t flags = encode(settings);
    const int written = fprintf(file, "%s %u %u\n", SETTINGS_MAGIC,
                                (unsigned)flags, (unsigned)(flags ^ SETTINGS_CHECK));
    const int flush_status = fflush(file);
    const int close_status = fclose(file);
    return written > 0 && flush_status == 0 && close_status == 0;
}
