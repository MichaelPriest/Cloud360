#include "menu.h"

static uint32_t wrap(uint32_t index, uint32_t count, bool backwards)
{
    return backwards ? (index + count - 1) % count : (index + 1) % count;
}

static bool has_content(uint32_t section)
{
    return section == CLOUD360_SECTION_HOME ||
           section == CLOUD360_SECTION_EXPLORE ||
           section == CLOUD360_SECTION_LIBRARY ||
           section == CLOUD360_SECTION_SETTINGS;
}

static void change_tab(Cloud360Menu *menu, bool backwards)
{
    menu->selected = wrap(menu->selected, CLOUD360_SECTION_COUNT, backwards);
    menu->card_index = 0;
    menu->setting_index = 0;
    menu->content_focus = false;
}

void cloud360_menu_init(Cloud360Menu *menu)
{
    menu->selected = CLOUD360_SECTION_HOME;
    menu->message_index = CLOUD360_NO_MESSAGE;
    menu->card_index = 0;
    menu->setting_index = 0;
    menu->catalog_count = 0;
    menu->previous_buttons = 0;
    menu->content_focus = false;
    menu->settings_dirty = false;
    menu->network_refresh_requested = false;
    menu->network_probe_requested = false;
    menu->network_https_requested = false;
    menu->catalog_local_requested = false;
    menu->catalog_online_requested = false;
    menu->storage_error = false;
    cloud360_settings_defaults(&menu->settings);
}

static void apply_setting(Cloud360Menu *menu)
{
    switch (menu->setting_index) {
        case 0: menu->settings.animations = !menu->settings.animations; break;
        case 1: menu->settings.ambient_effects = !menu->settings.ambient_effects; break;
        case 2: menu->settings.high_contrast = !menu->settings.high_contrast; break;
        case 3: menu->settings.show_hints = !menu->settings.show_hints; break;
        case 4: cloud360_settings_defaults(&menu->settings); break;
        default: return;
    }
    menu->settings_dirty = true;
}

bool cloud360_menu_step(Cloud360Menu *menu, uint16_t buttons)
{
    const uint16_t pressed = (uint16_t)(buttons & (uint16_t)~menu->previous_buttons);
    menu->previous_buttons = buttons;
    menu->settings_dirty = false;
    menu->network_refresh_requested = false;
    menu->network_probe_requested = false;
    menu->network_https_requested = false;
    menu->catalog_local_requested = false;
    menu->catalog_online_requested = false;

    if (pressed & CLOUD360_MENU_BACK)
        return true;

    if (menu->message_index != CLOUD360_NO_MESSAGE) {
        if (pressed & CLOUD360_MENU_CLOSE)
            menu->message_index = CLOUD360_NO_MESSAGE;
        return false;
    }

    if (pressed & CLOUD360_MENU_PREVIOUS_TAB) {
        change_tab(menu, true);
        return false;
    }
    if (pressed & CLOUD360_MENU_NEXT_TAB) {
        change_tab(menu, false);
        return false;
    }

    if ((pressed & CLOUD360_MENU_NETWORK_TEST) &&
        menu->selected == CLOUD360_SECTION_NETWORK) {
        menu->network_probe_requested = true;
        return false;
    }
    if ((pressed & CLOUD360_MENU_NETWORK_HTTPS) &&
        menu->selected == CLOUD360_SECTION_NETWORK) {
        menu->network_https_requested = true;
        return false;
    }

    if (menu->selected == CLOUD360_SECTION_EXPLORE) {
        if (pressed & CLOUD360_MENU_CATALOG_LOCAL) {
            menu->catalog_local_requested = true;
            return false;
        }
        if (pressed & CLOUD360_MENU_CATALOG_ONLINE) {
            menu->catalog_online_requested = true;
            return false;
        }
    }

    if (pressed & CLOUD360_MENU_CLOSE) {
        if (menu->content_focus)
            menu->content_focus = false;
        else if (menu->selected != CLOUD360_SECTION_HOME) {
            menu->selected = CLOUD360_SECTION_HOME;
            menu->card_index = 0;
            menu->setting_index = 0;
        }
        return false;
    }

    if (!menu->content_focus) {
        if (pressed & CLOUD360_MENU_LEFT)
            change_tab(menu, true);
        else if (pressed & CLOUD360_MENU_RIGHT)
            change_tab(menu, false);

        if (pressed & (CLOUD360_MENU_DOWN | CLOUD360_MENU_SELECT)) {
            if (has_content(menu->selected))
                menu->content_focus = true;
            else if (menu->selected == CLOUD360_SECTION_NETWORK) {
                if (pressed & CLOUD360_MENU_SELECT)
                    menu->network_refresh_requested = true;
            } else if (pressed & CLOUD360_MENU_SELECT)
                menu->message_index = menu->selected;
        }
        return false;
    }

    if (menu->selected == CLOUD360_SECTION_SETTINGS) {
        if (pressed & CLOUD360_MENU_UP) {
            if (menu->setting_index == 0)
                menu->content_focus = false;
            else
                menu->setting_index--;
        } else if (pressed & CLOUD360_MENU_DOWN) {
            menu->setting_index = wrap(menu->setting_index, CLOUD360_SETTING_COUNT, false);
        }
        if (menu->content_focus && (pressed & (CLOUD360_MENU_SELECT | CLOUD360_MENU_RIGHT)))
            apply_setting(menu);
        return false;
    }

    const uint32_t cards = menu->selected == CLOUD360_SECTION_EXPLORE &&
                           menu->catalog_count ? menu->catalog_count :
                           CLOUD360_CARD_COUNT;
    if (pressed & CLOUD360_MENU_UP) {
        menu->content_focus = false;
    } else if (pressed & CLOUD360_MENU_LEFT) {
        menu->card_index = wrap(menu->card_index, cards, true);
    } else if (pressed & CLOUD360_MENU_RIGHT) {
        menu->card_index = wrap(menu->card_index, cards, false);
    }

    if (menu->content_focus && (pressed & CLOUD360_MENU_SELECT))
        menu->message_index = menu->selected;
    return false;
}
