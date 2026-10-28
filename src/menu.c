#include "menu.h"
void cloud360_menu_init(Cloud360Menu *menu)
{
    menu->selected = 0;
    menu->message_index = CLOUD360_MENU_NO_MESSAGE;
    menu->previous_buttons = 0;
}
bool cloud360_menu_step(Cloud360Menu *menu, uint16_t buttons)
{
    const uint16_t pressed = (uint16_t)(buttons & (uint16_t)~menu->previous_buttons);
    menu->previous_buttons = buttons;
    if ((pressed & CLOUD360_MENU_BACK) != 0)
        return true;
    if ((pressed & CLOUD360_MENU_UP) != 0) {
        menu->selected = (menu->selected + CLOUD360_MENU_COUNT - 1) % CLOUD360_MENU_COUNT;
        menu->message_index = CLOUD360_MENU_NO_MESSAGE;
    } else if ((pressed & CLOUD360_MENU_DOWN) != 0) {
        menu->selected = (menu->selected + 1) % CLOUD360_MENU_COUNT;
        menu->message_index = CLOUD360_MENU_NO_MESSAGE;
    }
    if ((pressed & CLOUD360_MENU_CLOSE) != 0)
        menu->message_index = CLOUD360_MENU_NO_MESSAGE;
    else if ((pressed & CLOUD360_MENU_SELECT) != 0)
        menu->message_index = menu->selected;
    return false;
}
