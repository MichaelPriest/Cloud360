/*
 * Cloud360 — Xbox 360 native XEX prototype.
 *
 * This milestone adds a controller-navigable menu. It is not yet an xCloud
 * or Moonlight streaming client; menu selections explain the current limits.
 */

#include <stdbool.h>
#include <stdint.h>

#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/splash.h"
#include "xsdk/text.h"

enum {
    SPLASH_MS = 1500,
    BUFFER_COUNT = 2,
    SWAP_INTERVAL = 1,
    BEGIN_TIMEOUT_MS = 5000,
    CLEAR_ARGB = 0xFF101820,
    COLOR_TITLE = 0xFF75D5FF,
    COLOR_TEXT = 0xFFF1F5F9,
    COLOR_MUTED = 0xFF9AA9B8,
    COLOR_SELECTED = 0xFFFFC857,
    COLOR_RULE = 0xFF344452,
    MENU_COUNT = 5
};

static const char *const menu_items[MENU_COUNT] = {
    "Xbox Cloud Gaming (xCloud)",
    "PC Streaming (Moonlight)",
    "Biblioteca de jogos",
    "Configuracoes",
    "Sobre o Cloud360"
};

static const char *const message_lines[MENU_COUNT][2] = {
    {
        "O cliente xCloud ainda nao foi implementado.",
        "Esta opcao nao inicia uma sessao de jogo neste build."
    },
    {
        "A transmissao por PC ainda nao foi implementada.",
        "A compatibilidade com Moonlight sera avaliada separadamente."
    },
    {
        "O catalogo de jogos ainda nao esta conectado.",
        "Este prototipo nao lista nem inicia jogos."
    },
    {
        "As configuracoes ainda nao estao disponiveis.",
        "Esta versao demonstra apenas a navegacao pelo controle."
    },
    {
        "Prototipo independente para Xbox 360 RGH/JTAG.",
        "Nao afiliado nem endossado pela Microsoft ou Xbox."
    }
};

static void draw_menu(uint32_t framebuffer_address, uint32_t width, uint32_t height,
                      uint32_t selected, uint32_t message_index)
{
    xsdk_gpu_set_target(width, height);

    xsdk_text_draw(64, 46, 3, COLOR_TITLE, "CLOUD360");
    xsdk_text_draw(66, 116, 1, COLOR_TEXT, "DASHBOARD NATIVO PARA XBOX 360");
    xsdk_text_draw(66, 150, 1, COLOR_MUTED, "PROTOTIPO INICIAL | RGH/JTAG | INTERFACE EM DESENVOLVIMENTO");
    xsdk_text_draw(66, 190, 1, COLOR_RULE,
                   "---------------------------------------------------------------");

    for (uint32_t i = 0; i < MENU_COUNT; ++i) {
        const int32_t y = 232 + (int32_t)(i * 46);
        const uint32_t color = (i == selected) ? COLOR_SELECTED : COLOR_TEXT;
        xsdk_text_draw(72, y, 2, color, (i == selected) ? ">" : " ");
        xsdk_text_draw(106, y, 2, color, menu_items[i]);
    }

    xsdk_text_draw(66, 478, 1, COLOR_RULE,
                   "---------------------------------------------------------------");

    if (message_index < MENU_COUNT) {
        xsdk_text_draw(66, 516, 1, COLOR_SELECTED, message_lines[message_index][0]);
        xsdk_text_draw(66, 544, 1, COLOR_MUTED, message_lines[message_index][1]);
    } else {
        xsdk_text_draw(66, 516, 1, COLOR_TEXT, "Use o direcional para navegar e A para selecionar.");
        xsdk_text_draw(66, 544, 1, COLOR_MUTED, "As opcoes de streaming ainda nao estao operacionais.");
    }

    xsdk_text_draw(66, 632, 1, COLOR_MUTED, "BUILD PRELIMINAR - NENHUM STREAMING ATIVO");
    xsdk_text_draw(66, 668, 1, COLOR_TEXT, "D-PAD: navegar    A: selecionar    BACK: sair");

    xsdk_gpu_resolve(framebuffer_address, width, height, CLEAR_ARGB);
}

static void run_title(uint32_t width, uint32_t height)
{
    uint32_t selected = 0;
    uint32_t message_index = MENU_COUNT;
    uint16_t previous_buttons = 0;

    xsdk_log_write("Cloud360: menu running at %ux%u", (unsigned)width, (unsigned)height);

    for (;;) {
        INPUT_STATE input = { 0 };
        const uint32_t input_status = XamInputGetState(0, 0, &input);
        const uint16_t buttons = (input_status == ERROR_SUCCESS) ? input.gamepad.buttons : 0;
        const uint16_t pressed = (uint16_t)(buttons & (uint16_t)~previous_buttons);

        if ((pressed & INPUT_GAMEPAD_BACK) != 0) {
            xsdk_log_write("Cloud360: Back pressed; exiting");
            return;
        }

        if ((pressed & INPUT_GAMEPAD_DPAD_UP) != 0) {
            selected = (selected + MENU_COUNT - 1) % MENU_COUNT;
            message_index = MENU_COUNT;
        }
        if ((pressed & INPUT_GAMEPAD_DPAD_DOWN) != 0) {
            selected = (selected + 1) % MENU_COUNT;
            message_index = MENU_COUNT;
        }
        if ((pressed & INPUT_GAMEPAD_A) != 0) {
            message_index = selected;
            xsdk_log_write("Cloud360: menu option %u selected", (unsigned)(selected + 1));
        }

        previous_buttons = buttons;

        uint32_t framebuffer_address = 0;
        const NTSTATUS status = xsdk_present_begin(&framebuffer_address, BEGIN_TIMEOUT_MS);
        if (!NT_SUCCESS(status)) {
            xsdk_log_write("Cloud360: xsdk_present_begin failed: 0x%08X", (unsigned)status);
            return;
        }

        draw_menu(framebuffer_address, width, height, selected, message_index);
        xsdk_present_end();
    }
}

int main(void)
{
    xsdk_log_write("Cloud360: startup");

    const NTSTATUS splash_status = xsdk_splash_show(SPLASH_MS);
    if (!NT_SUCCESS(splash_status)) {
        xsdk_log_write("Cloud360: splash failed: 0x%08X", (unsigned)splash_status);
    }

    NTSTATUS status = xsdk_gpu_open();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: xsdk_gpu_open failed: 0x%08X", (unsigned)status);
        return 1;
    }

    status = xsdk_text_open();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: xsdk_text_open failed: 0x%08X", (unsigned)status);
        (void)xsdk_gpu_close();
        return 1;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    status = xsdk_present_open(XSDK_DISPLAY_720P, BUFFER_COUNT,
                               SWAP_INTERVAL, &width, &height);
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: xsdk_present_open failed: 0x%08X", (unsigned)status);
        xsdk_text_close();
        (void)xsdk_gpu_close();
        return 1;
    }

    run_title(width, height);

    xsdk_present_close();
    xsdk_text_close();

    status = xsdk_gpu_close();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: xsdk_gpu_close failed: 0x%08X", (unsigned)status);
    }

    xsdk_log_write("Cloud360: shutdown complete");
    return 0;
}
