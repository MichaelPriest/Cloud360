#ifndef CLOUD360_MENU_H
#define CLOUD360_MENU_H
#include <stdbool.h>
#include <stdint.h>
enum {
    CLOUD360_MENU_COUNT = 5,
    CLOUD360_MENU_NO_MESSAGE = CLOUD360_MENU_COUNT,
    CLOUD360_MENU_UP = 1u << 0,
    CLOUD360_MENU_DOWN = 1u << 1,
    CLOUD360_MENU_SELECT = 1u << 2,
    CLOUD360_MENU_CLOSE = 1u << 3,
    CLOUD360_MENU_BACK = 1u << 4
};
typedef struct {
    uint32_t selected;
    uint32_t message_index;
    uint16_t previous_buttons;
} Cloud360Menu;
void cloud360_menu_init(Cloud360Menu *menu);
bool cloud360_menu_step(Cloud360Menu *menu, uint16_t buttons);
#endif
