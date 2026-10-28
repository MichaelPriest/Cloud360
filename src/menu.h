#ifndef CLOUD360_MENU_H
#define CLOUD360_MENU_H

#include <stdbool.h>
#include <stdint.h>
#include "settings.h"

/* Navigation order matches the cloud-gaming-style top bar. */
enum {
    CLOUD360_SECTION_HOME = 0,
    CLOUD360_SECTION_EXPLORE,
    CLOUD360_SECTION_SEARCH,
    CLOUD360_SECTION_LIBRARY,
    CLOUD360_SECTION_SETTINGS,
    CLOUD360_SECTION_NETWORK,
    CLOUD360_SECTION_ABOUT,
    CLOUD360_SECTION_COUNT,
    CLOUD360_CARD_COUNT = 3,
    CLOUD360_SETTING_COUNT = 5,
    CLOUD360_NO_MESSAGE = CLOUD360_SECTION_COUNT
};

enum {
    CLOUD360_MENU_UP = 1u << 0,
    CLOUD360_MENU_DOWN = 1u << 1,
    CLOUD360_MENU_LEFT = 1u << 2,
    CLOUD360_MENU_RIGHT = 1u << 3,
    CLOUD360_MENU_SELECT = 1u << 4,
    CLOUD360_MENU_CLOSE = 1u << 5,
    CLOUD360_MENU_BACK = 1u << 6,
    CLOUD360_MENU_PREVIOUS_TAB = 1u << 7,
    CLOUD360_MENU_NEXT_TAB = 1u << 8,
    CLOUD360_MENU_NETWORK_TEST = 1u << 9
};

typedef struct {
    uint32_t selected;
    uint32_t message_index;
    uint32_t card_index;
    uint32_t setting_index;
    uint16_t previous_buttons;
    bool content_focus;
    bool settings_dirty;
    bool network_refresh_requested;
    bool network_probe_requested;
    bool storage_error;
    Cloud360Settings settings;
} Cloud360Menu;

void cloud360_menu_init(Cloud360Menu *menu);
bool cloud360_menu_step(Cloud360Menu *menu, uint16_t buttons);

#endif
