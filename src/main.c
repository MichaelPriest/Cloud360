/*
 * Cloud360 - prototype dashboard for Xbox 360 RGH/JTAG.
 * This build does not implement streaming, authentication, or Game Pass.
 */
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/splash.h"
#include "xsdk/text.h"

#include "menu.h"
#include "render.h"
#include "ui.h"

enum {
    SPLASH_MS = 1100,
    BUFFER_COUNT = 2,
    SWAP_INTERVAL = 1,
    BEGIN_TIMEOUT_MS = 5000
};

static uint32_t now_ms(void)
{
    struct timespec now = { 0 };
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (uint32_t)((uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000);
}

static uint16_t controller_buttons(void)
{
    INPUT_STATE input = { 0 };
    if (XamInputGetState(0, 0, &input) != ERROR_SUCCESS)
        return 0;

    const uint16_t raw = input.gamepad.buttons;
    uint16_t mapped = 0;
    if (raw & INPUT_GAMEPAD_DPAD_UP)
        mapped |= CLOUD360_MENU_UP;
    if (raw & INPUT_GAMEPAD_DPAD_DOWN)
        mapped |= CLOUD360_MENU_DOWN;
    if (raw & INPUT_GAMEPAD_A)
        mapped |= CLOUD360_MENU_SELECT;
    if (raw & INPUT_GAMEPAD_B)
        mapped |= CLOUD360_MENU_CLOSE;
    if (raw & INPUT_GAMEPAD_BACK)
        mapped |= CLOUD360_MENU_BACK;
    return mapped;
}

static void run_title(uint32_t width, uint32_t height)
{
    Cloud360Menu menu;
    Cloud360UiState ui;
    cloud360_menu_init(&menu);
    cloud360_ui_init(&ui);
    uint32_t last_ms = now_ms();

    xsdk_log_write("Cloud360: Azul Cloud360 dashboard %ux%u", (unsigned)width, (unsigned)height);

    for (;;) {
        const uint32_t prev_selected = menu.selected;
        const uint32_t prev_message = menu.message_index;
        if (cloud360_menu_step(&menu, controller_buttons())) {
            xsdk_log_write("Cloud360: Back pressed, exiting");
            return;
        }
        if (menu.selected != prev_selected)
            xsdk_log_write("Cloud360: selected %u", (unsigned)menu.selected);
        if (menu.message_index != prev_message)
            xsdk_log_write("Cloud360: info panel %u", (unsigned)menu.message_index);

        const uint32_t current_ms = now_ms();
        uint32_t dt_ms = current_ms - last_ms;
        last_ms = current_ms;
        if (dt_ms > 250)
            dt_ms = 16;
        cloud360_ui_step(&ui, &menu, dt_ms);

        uint32_t address = 0;
        const NTSTATUS status = xsdk_present_begin(&address, BEGIN_TIMEOUT_MS);
        if (!NT_SUCCESS(status)) {
            xsdk_log_write("Cloud360: xsdk_present_begin failed: 0x%08X", (unsigned)status);
            return;
        }

        cloud360_render(address, width, height, &menu, &ui);
        xsdk_present_end();
    }
}

int main(void)
{
    xsdk_log_write("Cloud360: startup");
    const NTSTATUS splash = xsdk_splash_show(SPLASH_MS);
    if (!NT_SUCCESS(splash))
        xsdk_log_write("Cloud360: splash failed 0x%08X", (unsigned)splash);

    NTSTATUS status = xsdk_gpu_open();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: gpu open failed 0x%08X", (unsigned)status);
        return 1;
    }

    status = xsdk_text_open();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: font open failed 0x%08X", (unsigned)status);
        (void)xsdk_gpu_close();
        return 1;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    status = xsdk_present_open(XSDK_DISPLAY_720P, BUFFER_COUNT, SWAP_INTERVAL,
                               &width, &height);
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: display open failed 0x%08X", (unsigned)status);
        xsdk_text_close();
        (void)xsdk_gpu_close();
        return 1;
    }

    run_title(width, height);

    xsdk_present_close();
    xsdk_text_close();
    status = xsdk_gpu_close();
    if (!NT_SUCCESS(status))
        xsdk_log_write("Cloud360: gpu close failed 0x%08X", (unsigned)status);
    xsdk_log_write("Cloud360: clean shutdown");
    return 0;
}
