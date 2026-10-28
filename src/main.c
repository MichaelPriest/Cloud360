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
#include "probe.h"
#include "https.h"
#include "catalog_runtime.h"
#include "oauth_runtime.h"
#include "xbl_runtime.h"
#include "session.h"
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
#define PROBE_HOST "www.xbox.com"
#define PROBE_PORT 443
#define HTTPS_HOST "example.com"
#define HTTPS_CA "game:/cacert.pem"
#define CATALOG_LOCAL_FILE "game:/catalog.json"
#define CATALOG_SOURCE_FILE "game:/catalog-source.txt"
#define OAUTH_CLIENT_FILE "game:/oauth-client.txt"

/* Detached XAM workers live for the whole title lifetime. */
static Cloud360Probe connectivity;
static Cloud360Https secure_http;
static Cloud360CatalogRuntime catalog_runtime;
static Cloud360OAuthRuntime microsoft_login;
static Cloud360XblRuntime xbox_authorization;
static Cloud360Session game_session;

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
    if (raw & INPUT_GAMEPAD_X)
        buttons |= CLOUD360_MENU_NETWORK_TEST |
                   CLOUD360_MENU_CATALOG_LOCAL |
                   CLOUD360_MENU_ACCOUNT_CANCEL |
                   CLOUD360_MENU_SESSION_STOP;
    if (raw & INPUT_GAMEPAD_Y)
        buttons |= CLOUD360_MENU_NETWORK_HTTPS |
                   CLOUD360_MENU_CATALOG_ONLINE |
                   CLOUD360_MENU_ACCOUNT_LOGOUT;
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
    cloud360_session_init(&game_session);

    Cloud360Network network;
    cloud360_network_init(&network);
    const Cloud360NetworkOps net_ops = {
        .link = console_link, .address = console_ipv4, .context = NULL
    };
    const Cloud360ProbeOps probe_ops = {
        .resolve_ipv4 = cloud360_probe_resolve_ipv4,
        .connect_tcp = cloud360_probe_connect_tcp,
        .context = NULL
    };
    const int probe_error = cloud360_probe_init(&connectivity);
    if (probe_error)
        xsdk_log_write("Cloud360: DNS/TCP diagnostic unavailable: %d", probe_error);

    const Cloud360HttpsTransport https_transport = {
        .fetch = cloud360_https_curl_fetch, .context = NULL
    };
    const Cloud360HttpsRequest https_request = {
        .host = HTTPS_HOST, .path = "/", .ca_file = HTTPS_CA
    };
    const int https_error = cloud360_https_init(&secure_http);
    if (https_error)
        xsdk_log_write("Cloud360: HTTPS diagnostic unavailable: %d", https_error);

    const Cloud360OAuthTransport login_transport = {
        .post = cloud360_https_curl_post_form, .context = NULL
    };
    const int login_error = cloud360_oauth_runtime_init(
        &microsoft_login, OAUTH_CLIENT_FILE, HTTPS_CA, login_transport);
    if (login_error)
        xsdk_log_write("Cloud360: OAuth state init failed: %d", login_error);
    const int xbl_error = cloud360_xbl_runtime_init(
        &xbox_authorization, HTTPS_CA, cloud360_https_curl_post_json, NULL);
    if (xbl_error)
        xsdk_log_write("Cloud360: XSTS state init failed: %d", xbl_error);

    const int catalog_error = cloud360_catalog_runtime_init(
        &catalog_runtime, cloud360_https_curl_get, NULL);
    if (catalog_error) {
        xsdk_log_write("Cloud360: catalog unavailable: %d", catalog_error);
    } else if (cloud360_catalog_runtime_local(
                   &catalog_runtime, CATALOG_LOCAL_FILE)) {
        xsdk_log_write("Cloud360: local catalog loaded");
    } else {
        xsdk_log_write("Cloud360: no valid local catalog; no games invented");
    }

    if (cloud360_settings_load(&menu.settings, SETTINGS_FILE))
        xsdk_log_write("Cloud360: preferences loaded");
    else
        xsdk_log_write("Cloud360: using default preferences");

    xsdk_log_write("Cloud360: xCloud-inspired native shell %ux%u",
                   (unsigned)width, (unsigned)height);

    uint32_t last_ms = milliseconds();
    for (;;) {
        Cloud360CatalogView catalog_view = { 0 };
        if (!catalog_error) {
            cloud360_catalog_runtime_snapshot(&catalog_runtime,
                                              menu.card_index, &catalog_view);
            menu.catalog_count = catalog_view.count;
            if (menu.selected == CLOUD360_SECTION_EXPLORE &&
                menu.catalog_count && menu.card_index >= menu.catalog_count)
                menu.card_index = 0;
        }
        const uint32_t prev_section = menu.selected;
        if (cloud360_menu_step(&menu, controller_buttons())) {
            xsdk_log_write("Cloud360: Back pressed - exit");
            if (!xbl_error)
                cloud360_xbl_runtime_cancel(&xbox_authorization);
            if (!login_error)
                cloud360_oauth_runtime_logout(&microsoft_login);
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

        if (!catalog_error && menu.selected == CLOUD360_SECTION_EXPLORE) {
            if (menu.catalog_local_requested)
                (void)cloud360_catalog_runtime_local(
                    &catalog_runtime, CATALOG_LOCAL_FILE);
            if (menu.catalog_online_requested)
                (void)cloud360_catalog_runtime_source(
                    &catalog_runtime, CATALOG_SOURCE_FILE, HTTPS_CA);
            cloud360_catalog_runtime_snapshot(&catalog_runtime,
                                              menu.card_index, &catalog_view);
            menu.catalog_count = catalog_view.count;
        }

        const uint32_t now = milliseconds();
        uint32_t delta = now - last_ms;
        last_ms = now;
        if (delta > 250)
            delta = 16;
        cloud360_ui_step(&ui, &menu, delta);

        /* OAuth access/refresh tokens remain in protected runtime memory.
         * The UI only receives the temporary user_code and verification URI.
         * Polling honors server timing and remains asynchronous. */
        if (!login_error) {
            cloud360_oauth_runtime_tick(&microsoft_login, now);
            if (menu.selected == CLOUD360_SECTION_ACCOUNT) {
                if (menu.account_cancel_requested) {
                    if (!xbl_error)
                        cloud360_xbl_runtime_cancel(&xbox_authorization);
                    cloud360_oauth_runtime_cancel(&microsoft_login);
                } else if (menu.account_logout_requested) {
                    if (!xbl_error)
                        cloud360_xbl_runtime_cancel(&xbox_authorization);
                    cloud360_oauth_runtime_logout(&microsoft_login);
                } else if (menu.account_begin_requested) {
                    Cloud360OAuthView current = { 0 };
                    cloud360_oauth_runtime_view(&microsoft_login, now, &current);
                    if (current.status == CLOUD360_OAUTH_AUTHORIZED) {
                        if (!xbl_error)
                            (void)cloud360_xbl_runtime_start(
                                &xbox_authorization, &microsoft_login);
                    } else {
                        if (!xbl_error)
                            cloud360_xbl_runtime_cancel(&xbox_authorization);
                        (void)cloud360_oauth_runtime_begin(&microsoft_login, now);
                    }
                }
            }
        }

        /* Session orchestration is intentionally fail-closed until an
         * authorized cloud transport + WebRTC/media backend exists.
         * No HTTP call, mock session or fake video is started here. */
        if (menu.selected == CLOUD360_SECTION_SESSION) {
            if (menu.session_stop_requested)
                cloud360_session_cancel(&game_session, now);
            else if (menu.session_start_requested) {
                Cloud360XblView current_xbl = { 0 };
                if (!xbl_error)
                    cloud360_xbl_runtime_snapshot(
                        &xbox_authorization, &current_xbl);
                /* A title from the local JSON is not considered playable. */
                (void)cloud360_session_begin(&game_session, NULL,
                    current_xbl.state == CLOUD360_XBL_AUTHORIZED,
                    false, now);
            }
        }

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
            if (!probe_error && menu.network_probe_requested &&
                network.status == CLOUD360_NET_LOCAL_IP &&
                cloud360_probe_start(&connectivity, &probe_ops,
                                     PROBE_HOST, PROBE_PORT))
                xsdk_log_write("Cloud360: starting independent DNS/TCP diagnostic");

            if (!https_error && menu.network_https_requested &&
                network.status == CLOUD360_NET_LOCAL_IP &&
                cloud360_https_start(&secure_http, &https_transport, &https_request))
                xsdk_log_write("Cloud360: HTTPS TLS diagnostic initiated");
        }

        uint32_t address = 0;
        const NTSTATUS status = xsdk_present_begin(&address, BEGIN_TIMEOUT_MS);
        if (!NT_SUCCESS(status)) {
            xsdk_log_write("Cloud360: present failed 0x%08X", (unsigned)status);
            if (!xbl_error)
                cloud360_xbl_runtime_cancel(&xbox_authorization);
            if (!login_error)
                cloud360_oauth_runtime_logout(&microsoft_login);
            return;
        }

        Cloud360ProbeSnapshot probe_result = { 0 };
        Cloud360HttpsSnapshot https_result = { 0 };
        if (!probe_error)
            cloud360_probe_snapshot(&connectivity, &probe_result);
        if (!https_error)
            cloud360_https_snapshot(&secure_http, &https_result);
        if (!catalog_error)
            cloud360_catalog_runtime_snapshot(&catalog_runtime,
                                              menu.card_index, &catalog_view);
        Cloud360OAuthView login_view = { 0 };
        if (!login_error)
            cloud360_oauth_runtime_view(&microsoft_login, now, &login_view);
        else
            login_view.status = CLOUD360_OAUTH_UNAVAILABLE;
        Cloud360XblView xbl_view = { 0 };
        if (!xbl_error)
            cloud360_xbl_runtime_snapshot(&xbox_authorization, &xbl_view);
        else
            xbl_view.state = CLOUD360_XBL_UNAVAILABLE;

        const bool xsts_ready = xbl_view.state == CLOUD360_XBL_AUTHORIZED;
        (void)cloud360_session_tick(&game_session, now, xsts_ready);
        Cloud360SessionView session_view = { 0 };
        cloud360_session_snapshot(&game_session, now, &session_view);

        cloud360_render(address, width, height, &menu, &ui,
                        &network, &probe_result, &https_result,
                        &catalog_view, &login_view, &xbl_view,
                        &session_view);
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
