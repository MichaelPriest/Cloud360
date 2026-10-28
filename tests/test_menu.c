#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/menu.h"

static void release(Cloud360Menu *menu)
{
    assert(!cloud360_menu_step(menu, 0));
}

static void press(Cloud360Menu *menu, uint16_t button)
{
    assert(!cloud360_menu_step(menu, button));
    release(menu);
}

static void test_top_tabs(void)
{
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    assert(menu.selected == CLOUD360_SECTION_HOME);
    assert(menu.message_index == CLOUD360_NO_MESSAGE);
    assert(menu.settings.animations && menu.settings.ambient_effects);
    press(&menu, CLOUD360_MENU_LEFT);
    assert(menu.selected == CLOUD360_SECTION_ABOUT);
    press(&menu, CLOUD360_MENU_RIGHT);
    assert(menu.selected == CLOUD360_SECTION_HOME);
    press(&menu, CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected == CLOUD360_SECTION_EXPLORE);
    press(&menu, CLOUD360_MENU_PREVIOUS_TAB);
    assert(menu.selected == CLOUD360_SECTION_HOME);
}

static void test_cards_and_modal(void)
{
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    press(&menu, CLOUD360_MENU_DOWN);
    assert(menu.content_focus && menu.card_index == 0);
    press(&menu, CLOUD360_MENU_RIGHT);
    assert(menu.card_index == 1);
    press(&menu, CLOUD360_MENU_LEFT);
    assert(menu.card_index == 0);
    press(&menu, CLOUD360_MENU_LEFT);
    assert(menu.card_index == CLOUD360_CARD_COUNT - 1);

    press(&menu, CLOUD360_MENU_SELECT);
    assert(menu.message_index == CLOUD360_SECTION_HOME);
    press(&menu, CLOUD360_MENU_RIGHT); /* Block input when a modal is shown. */
    assert(menu.card_index == CLOUD360_CARD_COUNT - 1);
    press(&menu, CLOUD360_MENU_CLOSE);
    assert(menu.message_index == CLOUD360_NO_MESSAGE);
    press(&menu, CLOUD360_MENU_CLOSE);
    assert(!menu.content_focus);
    press(&menu, CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected == CLOUD360_SECTION_EXPLORE);
    assert(menu.card_index == 0);
}

static void test_search_and_about(void)
{
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    press(&menu, CLOUD360_MENU_NEXT_TAB);
    press(&menu, CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected == CLOUD360_SECTION_SEARCH);
    press(&menu, CLOUD360_MENU_SELECT);
    assert(menu.message_index == CLOUD360_SECTION_SEARCH);
    press(&menu, CLOUD360_MENU_CLOSE);
    assert(menu.message_index == CLOUD360_NO_MESSAGE);
    press(&menu, CLOUD360_MENU_CLOSE);
    assert(menu.selected == CLOUD360_SECTION_HOME);
    assert(cloud360_menu_step(&menu, CLOUD360_MENU_BACK));
}

static void test_settings(void)
{
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    for (uint32_t n = 0; n < CLOUD360_SECTION_SETTINGS; ++n)
        press(&menu, CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected == CLOUD360_SECTION_SETTINGS);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SELECT));
    assert(menu.content_focus && menu.setting_index == 0);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SELECT));
    assert(menu.settings.animations); /* Held A must not toggle immediately. */
    release(&menu);

    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SELECT));
    assert(menu.settings_dirty);
    assert(!menu.settings.animations);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SELECT));
    assert(!menu.settings_dirty); /* Still held, no repeat. */
    release(&menu);

    press(&menu, CLOUD360_MENU_DOWN);
    assert(menu.setting_index == 1);
    press(&menu, CLOUD360_MENU_RIGHT);
    assert(!menu.settings.ambient_effects);
    press(&menu, CLOUD360_MENU_DOWN);
    press(&menu, CLOUD360_MENU_SELECT);
    assert(menu.settings.high_contrast);
    press(&menu, CLOUD360_MENU_DOWN);
    press(&menu, CLOUD360_MENU_SELECT);
    assert(!menu.settings.show_hints);
    press(&menu, CLOUD360_MENU_DOWN);
    assert(menu.setting_index == 4);
    press(&menu, CLOUD360_MENU_SELECT);
    assert(menu.settings.animations && menu.settings.ambient_effects);
    assert(!menu.settings.high_contrast && menu.settings.show_hints);
    press(&menu, CLOUD360_MENU_CLOSE);
    assert(!menu.content_focus);
}

static void test_network_refresh(void)
{
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    for (uint32_t n = 0; n < CLOUD360_SECTION_NETWORK; ++n)
        press(&menu, CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected == CLOUD360_SECTION_NETWORK);
    assert(!menu.content_focus);
    assert(!menu.network_refresh_requested);

    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SELECT));
    assert(menu.network_refresh_requested);
    assert(menu.message_index == CLOUD360_NO_MESSAGE);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SELECT));
    assert(!menu.network_refresh_requested);
    release(&menu);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SELECT));
    assert(menu.network_refresh_requested);
    release(&menu);

    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_NETWORK_TEST));
    assert(menu.network_probe_requested);
    assert(!menu.network_refresh_requested);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_NETWORK_TEST));
    assert(!menu.network_probe_requested);
    release(&menu);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_NETWORK_HTTPS));
    assert(menu.network_https_requested);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_NETWORK_HTTPS));
    assert(!menu.network_https_requested);
    release(&menu);
    press(&menu, CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected == CLOUD360_SECTION_ACCOUNT);
    assert(!menu.network_refresh_requested);
    assert(!menu.network_probe_requested);
    assert(!menu.network_https_requested);
}

static void test_catalog_navigation(void)
{
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    press(&menu, CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected == CLOUD360_SECTION_EXPLORE);

    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_CATALOG_LOCAL));
    assert(menu.catalog_local_requested);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_CATALOG_LOCAL));
    assert(!menu.catalog_local_requested);
    release(&menu);

    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_CATALOG_ONLINE));
    assert(menu.catalog_online_requested);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_CATALOG_ONLINE));
    assert(!menu.catalog_online_requested);
    release(&menu);

    menu.catalog_count = 7;
    press(&menu, CLOUD360_MENU_DOWN);
    assert(menu.content_focus);
    for (uint32_t i = 0; i < 7; ++i)
        press(&menu, CLOUD360_MENU_RIGHT);
    assert(menu.card_index == 0);
    press(&menu, CLOUD360_MENU_LEFT);
    assert(menu.card_index == 6);

    press(&menu, CLOUD360_MENU_NEXT_TAB);
    assert(!menu.content_focus);
    assert(menu.card_index == 0);
    assert(!menu.catalog_online_requested && !menu.catalog_local_requested);
}

static void test_account_login_actions(void)
{
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    for (uint32_t i=0; i<CLOUD360_SECTION_ACCOUNT; ++i)
        press(&menu,CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected==CLOUD360_SECTION_ACCOUNT);
    assert(!menu.account_begin_requested);

    assert(!cloud360_menu_step(&menu,CLOUD360_MENU_SELECT));
    assert(menu.account_begin_requested && !menu.content_focus);
    assert(!cloud360_menu_step(&menu,CLOUD360_MENU_SELECT));
    assert(!menu.account_begin_requested);
    release(&menu);

    assert(!cloud360_menu_step(&menu,CLOUD360_MENU_ACCOUNT_CANCEL));
    assert(menu.account_cancel_requested);
    release(&menu);
    assert(!cloud360_menu_step(&menu,CLOUD360_MENU_ACCOUNT_LOGOUT));
    assert(menu.account_logout_requested);
    release(&menu);
    press(&menu,CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected==CLOUD360_SECTION_SESSION);
    assert(!menu.account_cancel_requested && !menu.account_logout_requested);
    assert(!menu.account_begin_requested);
}

static void test_session_actions(void)
{
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    for (uint32_t i=0; i<CLOUD360_SECTION_SESSION; ++i)
        press(&menu, CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected == CLOUD360_SECTION_SESSION);
    assert(!menu.session_start_requested);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SELECT));
    assert(menu.session_start_requested);
    assert(!menu.content_focus);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SELECT));
    assert(!menu.session_start_requested);
    release(&menu);

    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SESSION_STOP));
    assert(menu.session_stop_requested);
    assert(!cloud360_menu_step(&menu, CLOUD360_MENU_SESSION_STOP));
    assert(!menu.session_stop_requested);
    release(&menu);

    press(&menu, CLOUD360_MENU_NEXT_TAB);
    assert(menu.selected == CLOUD360_SECTION_ABOUT);
    assert(!menu.session_start_requested && !menu.session_stop_requested);
    press(&menu, CLOUD360_MENU_PREVIOUS_TAB);
    assert(menu.selected == CLOUD360_SECTION_SESSION);
}

static void test_priority_and_focus(void)
{
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    press(&menu, CLOUD360_MENU_UP | CLOUD360_MENU_DOWN);
    assert(menu.content_focus);
    press(&menu, CLOUD360_MENU_UP);
    assert(!menu.content_focus);
    press(&menu, CLOUD360_MENU_PREVIOUS_TAB);
    assert(menu.selected == CLOUD360_SECTION_ABOUT);
    assert(cloud360_menu_step(&menu, CLOUD360_MENU_BACK));
}

int main(void)
{
    test_top_tabs();
    test_cards_and_modal();
    test_search_and_about();
    test_settings();
    test_network_refresh();
    test_catalog_navigation();
    test_account_login_actions();
    test_session_actions();
    test_priority_and_focus();
    puts("Cloud360 controller navigation: PASS");
    return 0;
}
