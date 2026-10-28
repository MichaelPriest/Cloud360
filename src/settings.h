#ifndef CLOUD360_SETTINGS_H
#define CLOUD360_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

/* UI-only preferences. No Microsoft credentials or other secrets are stored. */
typedef struct {
    bool animations;
    bool ambient_effects;
    bool high_contrast;
    bool show_hints;
} Cloud360Settings;

void cloud360_settings_defaults(Cloud360Settings *settings);
bool cloud360_settings_load(Cloud360Settings *settings, const char *path);
bool cloud360_settings_save(const Cloud360Settings *settings, const char *path);

#endif
