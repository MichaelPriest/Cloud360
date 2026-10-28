/*
 * Cloud360 native XSDK renderer.
 *
 * Phase 1 deliberately uses the SDK's GPU-backed bitmap font for both text
 * and colored panels. This avoids a dependency on external textures/shaders
 * while allowing theme, opacity and position animations on real XEX targets.
 * Layout is designed for the XSDK 1280x720 display.
 */
#include "render.h"

#include <stdint.h>
#include "xsdk/gpu.h"
#include "xsdk/text.h"

enum {
    BG = 0xFF0A1120,
    SIDEBAR = 0xFF101E34,
    HERO = 0xFF122B4D,
    HERO_BAND = 0xFF174477,
    HERO_BAND_2 = 0xFF185493,
    CARD = 0xFF15253D,
    CARD_ACTIVE = 0xFF1C385E,
    AZURE = 0xFF1677E8,
    SKY = 0xFF62C7FF,
    WHITE = 0xFFF4F8FF,
    MUTED = 0xFF91A5C1,
    LINE = 0xFF29405D,
    STATUS = 0xFF86E6C0,
    MAX_COLUMNS = 120,
    MAX_ROWS = 36
};

/* U+2588 FULL BLOCK is in XSDK's public-domain bitmap font. */
static void box(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t argb)
{
    uint32_t columns = (w + 9) / 10;
    uint32_t rows = (h + 19) / 20;
    if (!columns || !rows)
        return;
    if (columns > MAX_COLUMNS)
        columns = MAX_COLUMNS;
    if (rows > MAX_ROWS)
        rows = MAX_ROWS;

    static char text[MAX_ROWS * (MAX_COLUMNS * 3 + 1) + 1];
    char *out = text;
    for (uint32_t row = 0; row < rows; ++row) {
        for (uint32_t col = 0; col < columns; ++col) {
            *out++ = (char)0xE2;
            *out++ = (char)0x96;
            *out++ = (char)0x88;
        }
        if (row + 1 < rows)
            *out++ = '\n';
    }
    *out = '\0';
    xsdk_text_draw(x, y, 1, argb, text);
}

static uint32_t alpha(uint32_t color, uint8_t opacity)
{
    return (color & 0x00FFFFFFu) | ((uint32_t)opacity << 24);
}

static uint8_t scaled_alpha(uint8_t opacity, uint8_t level)
{
    return (uint8_t)((uint32_t)opacity * level / 255);
}

static const char *const nav_titles[CLOUD360_MENU_COUNT] = {
    "NUVEM", "PC", "JOGOS", "AJUSTE", "SOBRE"
};

static const char *const hero_titles[CLOUD360_MENU_COUNT] = {
    "XBOX CLOUD GAMING", "PC STREAMING", "BIBLIOTECA DE JOGOS",
    "CONFIGURACOES", "SOBRE O CLOUD360"
};

static const char *const hero_subtitles[CLOUD360_MENU_COUNT] = {
    "Seus jogos na nuvem. Em desenvolvimento para Xbox 360.",
    "Transmissao de um computador. Integracao nao disponivel.",
    "Uma biblioteca central. Catalogo ainda nao conectado.",
    "Personalizacao e rede. Funcionalidades futuras.",
    "Dashboard independente criada para Xbox 360 RGH/JTAG."
};

static const char *const modal_lines[CLOUD360_MENU_COUNT][2] = {
    { "O cliente xCloud ainda nao foi implementado.",
      "Esta versao nao estabelece sessoes na nuvem." },
    { "O streaming de PC ainda nao foi implementado.",
      "A compatibilidade Moonlight sera avaliada depois." },
    { "A biblioteca ainda nao possui catalogo online.",
      "Esta e apenas a primeira interface demonstrativa." },
    { "As configuracoes ainda nao estao disponiveis.",
      "O tema Azul Cloud360 esta ativo nesta versao." },
    { "Projeto de codigo aberto para Xbox 360 RGH/JTAG.",
      "Nao afiliado nem endossado pela Microsoft ou Xbox." }
};

static void draw_header(uint8_t intro)
{
    box(30, 34, 112, 640, SIDEBAR);
    box(48, 54, 74, 80, alpha(AZURE, scaled_alpha(intro, 220)));
    xsdk_text_draw(58, 68, 2, alpha(WHITE, intro), "C");
    xsdk_text_draw(62, 104, 1, alpha(WHITE, intro), "360");

    xsdk_text_draw(176, 46, 2, alpha(WHITE, intro), "CLOUD360");
    xsdk_text_draw(178, 98, 1, alpha(MUTED, intro),
                   "STREAMING HUB  /  DASHBOARD NATIVO");
    xsdk_text_draw(1050, 52, 1, alpha(SKY, intro), "XBOX 360");
    xsdk_text_draw(1050, 78, 1, alpha(MUTED, intro), "RGH / JTAG");
    box(170, 130, 1030, 4, LINE);
}

static void draw_sidebar(const Cloud360Menu *menu, const Cloud360UiState *ui, uint8_t intro)
{
    box(37, ui->cursor_y - 16, 98, 56,
        alpha(AZURE, scaled_alpha(intro, 222)));
    box(37, ui->cursor_y - 16, 10, 56,
        alpha(SKY, scaled_alpha(intro, 245)));
    for (uint32_t i = 0; i < CLOUD360_MENU_COUNT; ++i) {
        const int32_t y = CLOUD360_UI_NAV_TOP + (int32_t)i * CLOUD360_UI_NAV_STEP;
        const uint32_t color = i == menu->selected ? WHITE : MUTED;
        xsdk_text_draw(52, y, 1, alpha(color, intro), nav_titles[i]);
    }
}

static void draw_hero(const Cloud360Menu *menu, const Cloud360UiState *ui, uint8_t intro)
{
    box(170, 154, 1030, 220, HERO);
    box(170, 154, 1030, 20, alpha(HERO_BAND, 190));
    box(950, 174, 250, 200, alpha(HERO_BAND, 190));
    box(1070, 174, 130, 200, alpha(HERO_BAND_2, 170));
    box(1130, 214, 50, 120, alpha(SKY, cloud360_ui_ambient_alpha(ui)));
    box(190, 194, 10, 130, alpha(AZURE, scaled_alpha(intro, 240)));

    const uint8_t content_alpha = scaled_alpha(intro, cloud360_ui_content_alpha(ui));
    const int32_t offset = cloud360_ui_content_offset(ui);
    const uint32_t selected = menu->selected;

    xsdk_text_draw(218 + offset, 192, 1, alpha(SKY, content_alpha),
                   "CLOUD360  /  AREA SELECIONADA");
    xsdk_text_draw(215 + offset, 232, 2, alpha(WHITE, content_alpha),
                   hero_titles[selected]);
    xsdk_text_draw(218 + offset, 294, 1, alpha(WHITE, content_alpha),
                   hero_subtitles[selected]);
    xsdk_text_draw(218 + offset, 338, 1, alpha(SKY, content_alpha),
                   "A  VER DETALHES");
    xsdk_text_draw(1015, 242, 3, alpha(WHITE, scaled_alpha(intro, 55)),
                   "360");
}

static void draw_card(int32_t x, uint32_t index, const Cloud360Menu *menu,
                      const Cloud360UiState *ui, uint8_t intro)
{
    const bool focused = (menu->selected == index);
    const uint32_t bg_color = focused ? CARD_ACTIVE : CARD;
    box(x, 410, 320, 180, bg_color);
    box(x + 20, 430, 100, 20,
        alpha(focused ? SKY : AZURE, scaled_alpha(intro, focused ? 230 : 115)));
    if (focused) {
        const uint8_t glow = scaled_alpha(intro, (uint8_t)(120 + cloud360_ui_ambient_alpha(ui)));
        box(x, 410, 320, 8, alpha(SKY, glow));
        box(x, 570, 320, 8, alpha(AZURE, glow));
    }
    static const char *const names[] = {
        "XBOX CLOUD", "PC STREAMING", "BIBLIOTECA"
    };
    static const char *const subtitles[] = {
        "Jogos na nuvem", "Conecte seu PC", "Sua colecao"
    };
    xsdk_text_draw(x + 24, 468, 2, alpha(WHITE, intro), names[index]);
    xsdk_text_draw(x + 24, 528, 1, alpha(MUTED, intro), subtitles[index]);
    xsdk_text_draw(x + 270, 534, 1, alpha(focused ? SKY : MUTED, intro),
                   focused ? ">" : "+");
}

static void draw_footer(uint8_t intro)
{
    xsdk_text_draw(176, 616, 1, alpha(STATUS, intro), "INTERFACE DEMONSTRATIVA");
    xsdk_text_draw(450, 616, 1, alpha(MUTED, intro),
                   "Streaming e autenticacao ainda indisponiveis");
    box(170, 646, 1030, 4, LINE);
    xsdk_text_draw(176, 673, 1, alpha(MUTED, intro),
                   "D-PAD  NAVEGAR     A  ABRIR     B  FECHAR     BACK  SAIR");
}

static void draw_modal(const Cloud360Menu *menu, const Cloud360UiState *ui)
{
    if (menu->message_index >= CLOUD360_MENU_COUNT)
        return;
    const uint8_t a = cloud360_ui_modal_alpha(ui);
    box(170, 154, 1030, 440, alpha(BG, scaled_alpha(a, 210)));
    box(275, 236, 820, 260, alpha(SIDEBAR, scaled_alpha(a, 250)));
    box(275, 236, 820, 20, alpha(AZURE, scaled_alpha(a, 235)));
    xsdk_text_draw(310, 282, 2, alpha(WHITE, a),
                   hero_titles[menu->message_index]);
    xsdk_text_draw(310, 352, 1, alpha(WHITE, a),
                   modal_lines[menu->message_index][0]);
    xsdk_text_draw(310, 384, 1, alpha(MUTED, a),
                   modal_lines[menu->message_index][1]);
    xsdk_text_draw(310, 446, 1, alpha(SKY, a), "B  FECHAR  /  VOLTAR AO MENU");
}

void cloud360_render(uint32_t address, uint32_t width, uint32_t height,
                     const Cloud360Menu *menu, const Cloud360UiState *ui)
{
    xsdk_gpu_set_target(width, height);
    const uint8_t intro = cloud360_ui_intro_alpha(ui);
    draw_header(intro);
    draw_sidebar(menu, ui, intro);
    draw_hero(menu, ui, intro);
    draw_card(170, 0, menu, ui, intro);
    draw_card(525, 1, menu, ui, intro);
    draw_card(880, 2, menu, ui, intro);
    draw_footer(intro);
    draw_modal(menu, ui);

    /* xsdk_gpu_resolve clears eDRAM to BG for the NEXT frame. */
    xsdk_gpu_resolve(address, width, height, BG);
}
