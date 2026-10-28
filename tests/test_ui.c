#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/menu.h"
#include "../src/ui.h"

static void run_until(Cloud360UiState *ui, Cloud360Menu *menu,
                      uint32_t milliseconds)
{
    for (uint32_t i = 0; i < milliseconds / 16; ++i)
        cloud360_ui_step(ui, menu, 16);
}

static void test_intro_and_fixed_time(void)
{
    Cloud360UiState ui;
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    cloud360_ui_init(&ui);
    assert(ui.cursor_y == CLOUD360_UI_NAV_TOP);
    assert(cloud360_ui_intro_alpha(&ui) == 0);
    assert(cloud360_ui_content_alpha(&ui) == 0);
    run_until(&ui, &menu, 1000);
    assert(cloud360_ui_intro_alpha(&ui) == 255);
    assert(cloud360_ui_content_alpha(&ui) == 255);
    assert(cloud360_ui_content_offset(&ui) == 0);
}

static void test_eased_nav_and_interruption(void)
{
    Cloud360UiState ui;
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    cloud360_ui_init(&ui);
    menu.selected = 3;
    cloud360_ui_step(&ui, &menu, 16);
    const int32_t first = ui.cursor_y;
    assert(first > CLOUD360_UI_NAV_TOP);
    assert(first < CLOUD360_UI_NAV_TOP + 3 * CLOUD360_UI_NAV_STEP);
    assert(cloud360_ui_content_alpha(&ui) < 255);
    assert(cloud360_ui_content_offset(&ui) > 0);

    menu.selected = 1;  /* Direction changed during animation. */
    cloud360_ui_step(&ui, &menu, 16);
    assert(ui.cursor_y > CLOUD360_UI_NAV_TOP);
    assert(ui.cursor_y <= first + 2 * CLOUD360_UI_NAV_STEP);
    run_until(&ui, &menu, 320);
    assert(ui.cursor_y == CLOUD360_UI_NAV_TOP + CLOUD360_UI_NAV_STEP);
    assert(cloud360_ui_content_alpha(&ui) == 255);
}

static void test_modal_and_ambient(void)
{
    Cloud360UiState ui;
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    cloud360_ui_init(&ui);
    menu.message_index = 0;
    cloud360_ui_step(&ui, &menu, 16);
    assert(cloud360_ui_modal_alpha(&ui) > 0);
    assert(cloud360_ui_modal_alpha(&ui) < 255);
    run_until(&ui, &menu, 320);
    assert(cloud360_ui_modal_alpha(&ui) == 255);
    const uint8_t pulse = cloud360_ui_ambient_alpha(&ui);
    assert(pulse >= 18 && pulse <= 40);
    menu.message_index = CLOUD360_MENU_NO_MESSAGE;
    cloud360_ui_step(&ui, &menu, 16);
    assert(cloud360_ui_modal_alpha(&ui) == 0);
    menu.message_index = 2;
    cloud360_ui_step(&ui, &menu, 16);
    assert(cloud360_ui_modal_alpha(&ui) > 0);
    assert(cloud360_ui_modal_alpha(&ui) < 255);
}

static void test_large_elapsed_is_capped(void)
{
    Cloud360UiState ui;
    Cloud360Menu menu;
    cloud360_menu_init(&menu);
    cloud360_ui_init(&ui);
    menu.selected = 4;
    cloud360_ui_step(&ui, &menu, 1000000);
    assert(ui.cursor_ms == 64);
    assert(ui.cursor_y < CLOUD360_UI_NAV_TOP + 4 * CLOUD360_UI_NAV_STEP);
}

int main(void)
{
    test_intro_and_fixed_time();
    test_eased_nav_and_interruption();
    test_modal_and_ambient();
    test_large_elapsed_is_capped();
    puts("Cloud360 UI animation tests: PASS");
    return 0;
}
