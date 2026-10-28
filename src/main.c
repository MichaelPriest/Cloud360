/*
 * Cloud360 native Xbox 360 dashboard.
 * Experimental interface only. No Microsoft authentication or game streaming.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/net.h"
#include "xsdk/present.h"
#include "xsdk/splash.h"
#include "xsdk/text.h"

#include "menu.h"
#include "network.h"
#include "render.h"
#include "settings.h"
#include "ui.h"

enum {
    SPLASH_MS = 900,
    BUFFER_COUNT = 2,
    SWAP_INTERVAL = 1,
    BEGIN_TIMEOUT_MS = 5000,
    STICK_DEADZONE = 15000
};

#define SETTINGS_FILE "game:/cloud360.cfg"

static uint32_t milliseconds(void)
{
    struct timespec now = { 0 };
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (uint32_t)((uint64_t)now.tv_sec * 1000u +
                      (uint64_t)now.tv_nsec / 1000000u);
}

static uint16_t controller_buttons(void)
{
    INPUT_STATE input = { 0 };
    if (XamInputGetState(0, 0, &input) != ERROR_SUCCESS)
        return 0;

    const uint16_t raw = input.gamepad.buttons;
    const int16_t left_x = input.gamepad.thumb_lx;
    const int16_t left_y = input.gamepad.thumb_ly;
    uint16_t buttons = 0;

    if ((raw & INPUT_GAMEPAD_DPAD_UP) || left_y > STICK_DEADZONE)
        buttons |= CLOUD360_MENU_UP;
    if ((raw & INPUT_GAMEPAD_DPAD_DOWN) || left_y < -STICK_DEADZONE)
        buttons |= CLOUD360_MENU_DOWN;
    if ((raw & INPUT_GAMEPAD_DPAD_LEFT) || left_x < -STICK_DEADZONE)
        buttons |= CLOUD360_MENU_LEFT;
    if ((raw & INPUT_GAMEPAD_DPAD_RIGHT) || left_x > STICK_DEADZONE)
        buttons |= CLOUD360_MENU_RIGHT;
    if (raw & INPUT_GAMEPAD_A)
        buttons |= CLOUD360_MENU_SELECT;
    if (raw & INPUT_GAMEPAD_B)
        buttons |= CLOUD360_MENU_CLOSE;
    if (raw & INPUT_GAMEPAD_BACK)
        buttons |= CLOUD360_MENU_BACK;
    if (raw & INPUT_GAMEPAD_LEFT_SHOULDER)
        buttons |= CLOUD360_MENU_PREVIOUS_TAB;
    if (raw & INPUT_GAMEPAD_RIGHT_SHOULDER)
        buttons |= CLOUD360_MENU_NEXT_TAB;
    return buttons;
}

/* XSDK's XAM adapters. The state machine remains portable and unit-tested. */
static int console_link(void *context, uint32_t *flags)
{
    (void)context;
    return xsdk_net_link(flags);
}

static int console_ipv4(void *context, uint32_t *network_order_ip)
{
    (void)context;
    struct in_addr address = { 0 };
    const int error = xsdk_net_address(&address);
    if (!error)
        *network_order_ip = address.s_addr;
    return error;
}

static void run_title(uint32_t width, uint32_t height)
{
    Cloud360Menu menu;
    Cloud360UiState ui;
    cloud360_menu_init(&menu);
    cloud360_ui_init(&ui);

    Cloud360Network network;
    cloud360_network_init(&network);
    const Cloud360NetworkOps net_ops = {
        .link = console_link, .address = console_ipv4, .context = NULL
    };

    if (cloud360_settings_load(&menu.settings, SETTINGS_FILE))
        xsdk_log_write("Cloud360: preferences loaded");
    else
        xsdk_log_write("Cloud360: using default preferences");

    xsdk_log_write("Cloud360: xCloud-inspired native shell %ux%u",
                   (unsigned)width, (unsigned)height);

    uint32_t last_ms = milliseconds();
    for (;;) {
        const uint32_t prev_section = menu.selected;
        if (cloud360_menu_step(&menu, controller_buttons())) {
            xsdk_log_write("Cloud360: Back pressed - exit");
            return;
        }

        if (menu.settings_dirty) {
            menu.storage_error = !cloud360_settings_save(&menu.settings, SETTINGS_FILE);
            xsdk_log_write(menu.storage_error
                           ? "Cloud360: preferences changed (not persisted)"
                           : "Cloud360: preferences saved");
        }
        if (menu.selected != prev_section)
            xsdk_log_write("Cloud360: section %u", (unsigned)menu.selected);

        const uint32_t now = milliseconds();
        uint32_t delta = now - last_ms;
        last_ms = now;
        if (delta > 250)
            delta = 16;
        cloud360_ui_step(&ui, &menu, delta);

        /* Check XAM's network only while the user views the Network tab.
         * Polling is throttled, so rendering is not tied to network traffic. */
        if (menu.selected == CLOUD360_SECTION_NETWORK) {
            if (cloud360_network_poll(&network, &net_ops, now,
                                      menu.network_refresh_requested)) {
                xsdk_log_write("Cloud360: network: %s, IP %s, error %d",
                               cloud360_network_status_text(network.status),
                               network.ipv4[0] ? network.ipv4 : "-",
                               network.error);
            }
        }

        uint32_t address = 0;
        const NTSTATUS status = xsdk_present_begin(&address, BEGIN_TIMEOUT_MS);
        if (!NT_SUCCESS(status)) {
            xsdk_log_write("Cloud360: present failed 0x%08X", (unsigned)status);
            return;
        }

        cloud360_render(address, width, height, &menu, &ui, &network);
        xsdk_present_end();
    }
}

int main(void)
{
    xsdk_log_write("Cloud360: boot");
    const NTSTATUS splash = xsdk_splash_show(SPLASH_MS);
    if (!NT_SUCCESS(splash))
        xsdk_log_write("Cloud360: splash failed 0x%08X", (unsigned)splash);

    NTSTATUS status = xsdk_gpu_open();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: GPU unavailable 0x%08X", (unsigned)status);
        return 1;
    }

    status = xsdk_text_open();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: font unavailable 0x%08X", (unsigned)status);
        (void)xsdk_gpu_close();
        return 1;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    status = xsdk_present_open(XSDK_DISPLAY_720P, BUFFER_COUNT,
                               SWAP_INTERVAL, &width, &height);
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: display unavailable 0x%08X", (unsigned)status);
        xsdk_text_close();
        (void)xsdk_gpu_close();
        return 1;
    }

    run_title(width, height);

    xsdk_present_close();
    xsdk_text_close();
    status = xsdk_gpu_close();
    if (!NT_SUCCESS(status))
        xsdk_log_write("Cloud360: GPU close failed 0x%08X", (unsigned)status);
    xsdk_log_write("Cloud360: shutdown");
    return 0;
}
