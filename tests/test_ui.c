#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/menu.h"
#include "../src/ui.h"

static void frames(Cloud360UiState *ui, Cloud360Menu *menu, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
        cloud360_ui_step(ui, menu, 16);
}

static void test_intro_and_switch(void)
{
    Cloud360Menu menu;
    Cloud360UiState ui;
    cloud360_menu_init(&menu);
    cloud360_ui_init(&ui);
    assert(ui.tab_x == CLOUD360_UI_TAB_X);
    assert(cloud360_ui_intro_alpha(&ui) == 0);
    frames(&ui, &menu, 35);
    assert(cloud360_ui_intro_alpha(&ui) == 255);
    menu.selected = CLOUD360_SECTION_LIBRARY;
    cloud360_ui_step(&ui, &menu, 16);
    assert(ui.tab_x > CLOUD360_UI_TAB_X);
    assert(ui.tab_x < CLOUD360_UI_TAB_X + CLOUD360_SECTION_LIBRARY * CLOUD360_UI_TAB_STEP);
    assert(cloud360_ui_page_alpha(&ui) < 255);
    assert(cloud360_ui_page_offset(&ui) > 0);
    menu.selected = CLOUD360_SECTION_SEARCH;
    cloud360_ui_step(&ui, &menu, 16);
    frames(&ui, &menu, 20);
    assert(ui.tab_x == CLOUD360_UI_TAB_X + CLOUD360_SECTION_SEARCH * CLOUD360_UI_TAB_STEP);
    assert(cloud360_ui_page_alpha(&ui) == 255);
}

static void test_setting_motion(void)
{
    Cloud360Menu menu;
    Cloud360UiState ui;
    cloud360_menu_init(&menu);
    cloud360_ui_init(&ui);
    menu.selected = CLOUD360_SECTION_SETTINGS;
    menu.setting_index = 3;
    cloud360_ui_step(&ui, &menu, 16);
    assert(ui.setting_y > CLOUD360_UI_SETTINGS_Y);
    assert(ui.setting_y < CLOUD360_UI_SETTINGS_Y + 3 * CLOUD360_UI_SETTINGS_STEP);
    frames(&ui, &menu, 20);
    assert(ui.setting_y == CLOUD360_UI_SETTINGS_Y + 3 * CLOUD360_UI_SETTINGS_STEP);
}

static void test_modal_and_ambient(void)
{
    Cloud360Menu menu;
    Cloud360UiState ui;
    cloud360_menu_init(&menu);
    cloud360_ui_init(&ui);
    menu.message_index = CLOUD360_SECTION_HOME;
    cloud360_ui_step(&ui, &menu, 16);
    assert(cloud360_ui_modal_alpha(&ui) > 0 && cloud360_ui_modal_alpha(&ui) < 255);
    frames(&ui, &menu, 20);
    assert(cloud360_ui_modal_alpha(&ui) == 255);
    assert(cloud360_ui_ambient_alpha(&ui) >= 15);
    menu.message_index = CLOUD360_NO_MESSAGE;
    cloud360_ui_step(&ui, &menu, 16);
    assert(cloud360_ui_modal_alpha(&ui) == 0);
    menu.settings.ambient_effects = false;
    cloud360_ui_step(&ui, &menu, 16);
    assert(ui.ambient_ms == 0);
}

static void test_disable_animations(void)
{
    Cloud360Menu menu;
    Cloud360UiState ui;
    cloud360_menu_init(&menu);
    cloud360_ui_init(&ui);
    menu.settings.animations = false;
    menu.selected = CLOUD360_SECTION_ABOUT;
    menu.message_index = CLOUD360_SECTION_ABOUT;
    cloud360_ui_step(&ui, &menu, 16);
    assert(cloud360_ui_page_alpha(&ui) == 255);
    assert(cloud360_ui_modal_alpha(&ui) == 255);
    assert(cloud360_ui_intro_alpha(&ui) == 255);
    assert(ui.tab_x == CLOUD360_UI_TAB_X + CLOUD360_SECTION_ABOUT * CLOUD360_UI_TAB_STEP);
    assert(cloud360_ui_page_offset(&ui) == 0);
    menu.selected = CLOUD360_SECTION_HOME;
    cloud360_ui_step(&ui, &menu, 1000000);
    assert(ui.tab_x == CLOUD360_UI_TAB_X);
}

int main(void)
{
    test_intro_and_switch();
    test_setting_motion();
    test_modal_and_ambient();
    test_disable_animations();
    puts("Cloud360 UI easing and settings: PASS");
    return 0;
}
