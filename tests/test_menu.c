#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/menu.h"
static void release(Cloud360Menu *menu)
{
    assert(!cloud360_menu_step(menu, 0));
}
static void test_navigation(void)
{
    Cloud360Menu m;
    cloud360_menu_init(&m);
    assert(m.selected == 0 && m.message_index == CLOUD360_MENU_NO_MESSAGE);
    assert(!cloud360_menu_step(&m, CLOUD360_MENU_UP));
    assert(m.selected == CLOUD360_MENU_COUNT - 1);
    release(&m);
    for (uint32_t i = 0; i < CLOUD360_MENU_COUNT; ++i) {
        assert(!cloud360_menu_step(&m, CLOUD360_MENU_DOWN));
        release(&m);
    }
    assert(m.selected == CLOUD360_MENU_COUNT - 1);
    assert(!cloud360_menu_step(&m, CLOUD360_MENU_DOWN));
    assert(!cloud360_menu_step(&m, CLOUD360_MENU_DOWN));
    assert(m.selected == 0);
    release(&m);
    assert(!cloud360_menu_step(&m, CLOUD360_MENU_DOWN));
    assert(m.selected == 1);
}
static void test_selection(void)
{
    Cloud360Menu m;
    cloud360_menu_init(&m);
    assert(!cloud360_menu_step(&m, CLOUD360_MENU_SELECT));
    assert(m.message_index == 0);
    release(&m);
    assert(!cloud360_menu_step(&m, CLOUD360_MENU_CLOSE));
    assert(m.message_index == CLOUD360_MENU_NO_MESSAGE);
    release(&m);
    assert(!cloud360_menu_step(&m, CLOUD360_MENU_DOWN));
    release(&m);
    assert(!cloud360_menu_step(&m, CLOUD360_MENU_SELECT));
    assert(m.message_index == 1);
    release(&m);
    assert(!cloud360_menu_step(&m, CLOUD360_MENU_UP));
    assert(m.message_index == CLOUD360_MENU_NO_MESSAGE);
}
static void test_exit_and_priority(void)
{
    Cloud360Menu m;
    cloud360_menu_init(&m);
    assert(!cloud360_menu_step(&m, CLOUD360_MENU_UP | CLOUD360_MENU_DOWN));
    assert(m.selected == CLOUD360_MENU_COUNT - 1);
    release(&m);
    assert(cloud360_menu_step(&m, CLOUD360_MENU_BACK));
}
int main(void)
{
    test_navigation();
    test_selection();
    test_exit_and_priority();
    puts("Cloud360 menu tests: PASS");
    return 0;
}
