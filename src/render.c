/*
 * Cloud360 0.2: console-style shell inspired by the layout of cloud-gaming
 * dashboards. Icons are original 6x6 pixel art drawn with XSDK's bitmap font.
 * Not a Microsoft UI replica and not a functional streaming client yet.
 */
#include "render.h"

#include <stdbool.h>
#include <stdint.h>
#include "icons.h"
#include "xsdk/gpu.h"
#include "xsdk/text.h"

enum {
    BACKGROUND = 0xFF080E19,
    SURFACE = 0xFF101B2D,
    SURFACE_BRIGHT = 0xFF192A43,
    HERO = 0xFF102E50,
    HERO_ACCENT = 0xFF174778,
    HERO_STRIPE = 0xFF28649F,
    AZURE = 0xFF1677E8,
    SKY = 0xFF6DD5FF,
    WHITE = 0xFFF7FAFF,
    MUTED = 0xFF9FB1C9,
    OUTLINE = 0xFF314660,
    GREEN = 0xFF8ADCC3,
    GOLD = 0xFFF4C66C,
    MAX_COLUMNS = 120,
    MAX_ROWS = 36
};

static uint32_t fade(uint32_t color, uint8_t opacity)
{
    return (color & 0x00FFFFFFu) | ((uint32_t)opacity << 24);
}
static uint8_t mix_alpha(uint8_t a, uint8_t b)
{
    return (uint8_t)((uint32_t)a * b / 255);
}

/* XSDK's font includes U+2588. Rectangle edges are quantized to 10x20 px. */
static void rect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (!w || !h)
        return;
    uint32_t cols = (w + 9) / 10;
    uint32_t rows = (h + 19) / 20;
    if (cols > MAX_COLUMNS) cols = MAX_COLUMNS;
    if (rows > MAX_ROWS) rows = MAX_ROWS;

    static char glyphs[MAX_ROWS * (MAX_COLUMNS * 3 + 1) + 1];
    char *s = glyphs;
    for (uint32_t row = 0; row < rows; ++row) {
        for (uint32_t col = 0; col < cols; ++col) {
            *s++ = (char)0xE2;
            *s++ = (char)0x96;
            *s++ = (char)0x88;
        }
        if (row + 1 < rows)
            *s++ = '\n';
    }
    *s = 0;
    xsdk_text_draw(x, y, 1, color, glyphs);
}

/* Draws a light 3px horizontal line using U+2581 (lower one-eighth). */
static void underline(int32_t x, int32_t y, uint32_t width, uint32_t color)
{
    uint32_t cols = (width + 9) / 10;
    if (cols > MAX_COLUMNS) cols = MAX_COLUMNS;
    char glyphs[MAX_COLUMNS * 3 + 1];
    char *s = glyphs;
    for (uint32_t i = 0; i < cols; ++i) {
        *s++ = (char)0xE2;
        *s++ = (char)0x96;
        *s++ = (char)0x81;
    }
    *s = 0;
    xsdk_text_draw(x, y, 1, color, glyphs);
}

static void label(int32_t x, int32_t y, uint32_t scale,
                  uint32_t color, uint8_t opacity, const char *value)
{
    xsdk_text_draw(x, y, scale, fade(color, opacity), value);
}

static uint32_t contrast_text(const Cloud360Menu *menu)
{
    return menu->settings.high_contrast ? WHITE : MUTED;
}

static uint32_t contrast_panel(const Cloud360Menu *menu)
{
    return menu->settings.high_contrast ? SURFACE_BRIGHT : SURFACE;
}

static const char *const tabs[CLOUD360_SECTION_COUNT] = {
    "INICIO", "EXPLORAR", "BUSCAR", "BIBLIOTECA", "AJUSTES", "REDE", "SOBRE"
};

static const char *const hero_titles[CLOUD360_SECTION_COUNT] = {
    "SUA NUVEM. SEU CONSOLE.",
    "DESCUBRA NOVOS JOGOS",
    "PESQUISAR JOGOS",
    "SUA BIBLIOTECA",
    "PERSONALIZE SUA EXPERIENCIA",
    "STATUS DA CONEXAO",
    "O FUTURO NO XBOX 360"
};

static const char *const hero_lines[CLOUD360_SECTION_COUNT] = {
    "Um novo jeito de explorar jogos no Xbox 360.",
    "Catalogo sera exibido apos a integracao online.",
    "Busca disponivel quando o catalogo estiver conectado.",
    "Seus favoritos e jogos aparecerão apos o login.",
    "Ajustes visuais disponiveis nesta versao.",
    "Diagnostico real da rede local do console.",
    "Projeto independente, feito para Xbox 360 RGH/JTAG."
};

static const Cloud360Icon tab_icons[CLOUD360_SECTION_COUNT] = {
    CLOUD360_ICON_HOME, CLOUD360_ICON_GAMEPAD, CLOUD360_ICON_SEARCH,
    CLOUD360_ICON_LIBRARY, CLOUD360_ICON_SETTINGS,
    CLOUD360_ICON_PC, CLOUD360_ICON_INFO
};

static void header(const Cloud360Menu *menu, const Cloud360UiState *ui)
{
    const uint8_t a = cloud360_ui_intro_alpha(ui);
    rect(36, 28, 68, 68, fade(AZURE, mix_alpha(a, 210)));
    cloud360_icon_draw(40, 32, CLOUD360_ICON_CLOUD, fade(WHITE, a));
    label(122, 32, 2, WHITE, a, "CLOUD360");
    label(123, 80, 1, SKY, a, "CLOUD GAMING PARA XBOX 360");
    label(1145, 40, 1, GREEN, a, "RGH");
    label(1140, 68, 1, MUTED, a, "BETA 0.2");

    underline(32, 108, 1210, OUTLINE);
    for (uint32_t i = 0; i < CLOUD360_SECTION_COUNT; ++i) {
        const int32_t x = CLOUD360_UI_TAB_X + (int32_t)i * CLOUD360_UI_TAB_STEP;
        const uint32_t color = i == menu->selected ? WHITE : contrast_text(menu);
        label(x, 62, 1, color, a, tabs[i]);
    }
    underline(ui->tab_x, 108, 98, AZURE);
    if (!menu->content_focus)
        label(ui->tab_x, 111, 1, SKY, a, "\xE2\x96\xB4");
}

static void hero(const Cloud360Menu *menu, const Cloud360UiState *ui)
{
    const uint8_t a = mix_alpha(cloud360_ui_intro_alpha(ui), cloud360_ui_page_alpha(ui));
    const int32_t offset = cloud360_ui_page_offset(ui);

    rect(42, 146, 1192, 226, HERO);
    rect(42, 146, 1192, 20, HERO_ACCENT);
    rect(945, 166, 289, 206, fade(HERO_STRIPE, 120));
    rect(1105, 166, 129, 206, fade(AZURE, 110));

    label(78 + offset, 184, 1, SKY, a, "XBOX CLOUD GAMING  /  CONCEITO PRELIMINAR");
    label(75 + offset, 228, 2, WHITE, a, hero_titles[menu->selected]);
    label(78 + offset, 300, 1, WHITE, a, hero_lines[menu->selected]);
    label(78 + offset, 340, 1, GREEN, a, "● UI NATIVA  •  STREAMING AINDA INDISPONIVEL");

    const uint8_t glow = menu->settings.ambient_effects ? cloud360_ui_ambient_alpha(ui) : 0;
    rect(1025, 200, 150, 128, fade(SKY, mix_alpha(a, (uint8_t)(30 + glow))));
    cloud360_icon_draw(1066, 232, tab_icons[menu->selected], fade(WHITE, a));
}

static const char *const cards_home[CLOUD360_CARD_COUNT] = {
    "JOGOS NA NUVEM", "CATALOGO", "SUA COLECAO"
};
static const char *const cards_explore[CLOUD360_CARD_COUNT] = {
    "TODOS OS JOGOS", "GENEROS", "DESTAQUES"
};
static const char *const cards_library[CLOUD360_CARD_COUNT] = {
    "FAVORITOS", "RECENTES", "MINHA LISTA"
};
static const char *const card_descriptions[CLOUD360_CARD_COUNT] = {
    "Disponivel em breve", "Integracao futura", "Sem catalogo no momento"
};
static const Cloud360Icon card_icons[CLOUD360_CARD_COUNT] = {
    CLOUD360_ICON_CLOUD, CLOUD360_ICON_GAMEPAD, CLOUD360_ICON_LIBRARY
};

static const char *card_title(const Cloud360Menu *menu, uint32_t index)
{
    if (menu->selected == CLOUD360_SECTION_EXPLORE)
        return cards_explore[index];
    if (menu->selected == CLOUD360_SECTION_LIBRARY)
        return cards_library[index];
    return cards_home[index];
}

static void card(const Cloud360Menu *menu, const Cloud360UiState *ui,
                 uint32_t index, uint8_t a)
{
    const int32_t x = 42 + (int32_t)index * 405;
    const int32_t y = 426;
    const bool active = menu->content_focus && menu->card_index == index;
    const uint32_t bg = active ? SURFACE_BRIGHT : contrast_panel(menu);
    const uint8_t focus_a = mix_alpha(a, cloud360_ui_focus_alpha(ui));

    rect(x, y, 380, 190, bg);
    rect(x, y, 380, 20, fade(active ? SKY : AZURE, active ? focus_a : 120));
    if (active) {
        rect(x, y + 176, 380, 20, fade(AZURE, focus_a));
        rect(x, y, 10, 190, fade(SKY, focus_a));
    }
    rect(x + 20, y + 36, 90, 96, fade(AZURE, active ? 180 : 100));
    cloud360_icon_draw(x + 35, y + 54, card_icons[index], fade(WHITE, a));
    label(x + 130, y + 56, 1, WHITE, a, card_title(menu, index));
    label(x + 130, y + 95, 1, contrast_text(menu), a, card_descriptions[index]);
    label(x + 25, y + 155, 1, active ? SKY : MUTED, a,
          active ? "A  VISUALIZAR" : "EM PREPARACAO");
}

static void cards(const Cloud360Menu *menu, const Cloud360UiState *ui)
{
    const uint8_t a = mix_alpha(cloud360_ui_page_alpha(ui), cloud360_ui_intro_alpha(ui));
    label(44, 386, 1, WHITE, a, menu->selected == CLOUD360_SECTION_HOME
          ? "EXPLORE SEU CLOUD360" : "CATEGORIAS");
    if (menu->content_focus)
        label(920, 386, 1, SKY, a, "<  ESCOLHER  >");
    for (uint32_t index = 0; index < CLOUD360_CARD_COUNT; ++index)
        card(menu, ui, index, a);
}

static void search_page(const Cloud360Menu *menu, const Cloud360UiState *ui)
{
    const uint8_t a = cloud360_ui_page_alpha(ui);
    rect(42, 432, 1192, 160, contrast_panel(menu));
    cloud360_icon_draw(80, 452, CLOUD360_ICON_SEARCH, fade(SKY, a));
    label(164, 471, 2, WHITE, a, "BUSCAR NO CATALOGO");
    label(164, 536, 1, contrast_text(menu), a,
          "A pesquisa sera ativada apos integrarmos autenticacao e catalogo.");
}

static const char *const setting_names[CLOUD360_SETTING_COUNT] = {
    "ANIMACOES SUAVES", "ILUMINACAO AMBIENTE", "ALTO CONTRASTE",
    "DICAS NA TELA", "RESTAURAR PADROES"
};
static const char *const setting_details[CLOUD360_SETTING_COUNT] = {
    "Transicoes leves entre telas e cards",
    "Brilho discreto no plano de fundo",
    "Texto mais destacado para sua TV",
    "Comandos exibidos no rodape",
    "Voltar ao tema Azul Cloud360 original"
};
static const Cloud360Icon setting_icons[CLOUD360_SETTING_COUNT] = {
    CLOUD360_ICON_SPARK, CLOUD360_ICON_CLOUD, CLOUD360_ICON_STAR,
    CLOUD360_ICON_INFO, CLOUD360_ICON_SETTINGS
};

static bool setting_enabled(const Cloud360Menu *menu, uint32_t index)
{
    switch(index) {
        case 0: return menu->settings.animations;
        case 1: return menu->settings.ambient_effects;
        case 2: return menu->settings.high_contrast;
        case 3: return menu->settings.show_hints;
        default: return false;
    }
}

static void settings_page(const Cloud360Menu *menu, const Cloud360UiState *ui)
{
    const uint8_t a = cloud360_ui_page_alpha(ui);
    label(52, 150, 2, WHITE, a, "CONFIGURACOES");
    label(54, 200, 1, contrast_text(menu), a,
          "D-PAD: ESCOLHER   A: ALTERAR   B: VOLTAR");

    for (uint32_t index = 0; index < CLOUD360_SETTING_COUNT; ++index) {
        const int32_t y = CLOUD360_UI_SETTINGS_Y + (int32_t)index * CLOUD360_UI_SETTINGS_STEP;
        const bool active = menu->content_focus && menu->setting_index == index;
        rect(50, y, 1170, 62, active ? SURFACE_BRIGHT : contrast_panel(menu));
        if (active)
            rect(50, ui->setting_y, 10, 60, AZURE);
        cloud360_icon_draw(78, y, setting_icons[index], fade(active ? SKY : MUTED, a));
        label(165, y + 6, 1, WHITE, a, setting_names[index]);
        label(165, y + 33, 1, contrast_text(menu), a, setting_details[index]);
        if (index == CLOUD360_SETTING_COUNT - 1) {
            label(1060, y + 23, 1, SKY, a, "REDEFINIR");
        } else {
            const bool on = setting_enabled(menu, index);
            rect(1065, y + 20, 114, 26, fade(on ? AZURE : OUTLINE, a));
            label(1094, y + 21, 1, WHITE, a, on ? "LIGADO" : "DESLIG.");
        }
    }
    label(54, 616, 1, menu->storage_error ? GOLD : GREEN, a,
          menu->storage_error
          ? "Nao foi possivel salvar no dispositivo. Ajustes ativos ate sair."
          : "Preferencias salvas na pasta do aplicativo, quando disponivel.");
}

/* XSDK queries XAM's real Ethernet/link and IPv4 state. An IP address does
 * NOT mean Microsoft authentication, internet access, or xCloud support. */
/* DNS/TCP proves only transport reachability, not Xbox Live or xCloud. */
static void network_page(const Cloud360Menu *menu, const Cloud360UiState *ui,
                         const Cloud360Network *net,
                         const Cloud360ProbeSnapshot *probe)
{
    const uint8_t a = cloud360_ui_page_alpha(ui);
    rect(42, 390, 1192, 226, contrast_panel(menu));
    const bool has_ip = net->status == CLOUD360_NET_LOCAL_IP;
    const bool link = (net->link_flags & 0x01u) != 0;
    const uint32_t local_color = has_ip ? GREEN :
                                 net->status == CLOUD360_NET_ERROR ? GOLD : SKY;
    const bool tcp_ok = probe->status == CLOUD360_PROBE_CONNECTED;
    const bool failed = probe->status == CLOUD360_PROBE_DNS_ERROR ||
                        probe->status == CLOUD360_PROBE_TCP_ERROR ||
                        probe->status == CLOUD360_PROBE_THREAD_ERROR;
    const uint32_t remote_color = tcp_ok ? GREEN : failed ? GOLD : SKY;

    cloud360_icon_draw(72, 412, CLOUD360_ICON_PC, fade(local_color, a));
    label(160, 404, 2, WHITE, a, "REDE DO XBOX 360");
    label(900, 419, 1, MUTED, a, "www.xbox.com:443");
    label(165, 459, 1, WHITE, a, "LOCAL:");
    label(280, 459, 1, local_color, a,
          cloud360_network_status_text(net->status));
    label(165, 489, 1, contrast_text(menu), a, "LINK:");
    label(280, 489, 1, local_color, a,
          link ? "ATIVO" : "INATIVO / INDEFINIDO");
    label(516, 489, 1, contrast_text(menu), a, "IPV4:");
    label(610, 489, 1, has_ip ? GREEN : MUTED, a,
          has_ip ? net->ipv4 : "NAO DISPONIVEL");
    label(165, 526, 1, contrast_text(menu), a, "DNS/TCP:");
    label(309, 526, 1, remote_color, a,
          cloud360_probe_status_text(probe->status));
    label(165, 568, 1, SKY, a, "A  ATUALIZAR REDE       X  TESTAR DNS + TCP");
    label(165, 593, 1, contrast_text(menu), a,
          "TCP 443 nao valida TLS, Xbox Live, login ou streaming.");
}

static void about_page(const Cloud360Menu *menu, const Cloud360UiState *ui)
{
    const uint8_t a = cloud360_ui_page_alpha(ui);
    rect(42, 426, 1192, 182, contrast_panel(menu));
    cloud360_icon_draw(72, 460, CLOUD360_ICON_INFO, fade(SKY, a));
    label(165, 442, 2, WHITE, a, "CLOUD360  0.2.0.0");
    label(165, 505, 1, contrast_text(menu), a,
          "Interface nativa Xbox 360 RGH/JTAG. Projeto comunitario independente.");
    label(165, 545, 1, GREEN, a,
          "Proxima etapa: login, catalogo real e pesquisa de viabilidade do streaming.");
    label(165, 579, 1, MUTED, a,
          "Nao afiliado a Microsoft, Xbox ou Game Pass.");
}

static const char *const modal_messages[CLOUD360_SECTION_COUNT][2] = {
    { "O acesso ao xCloud ainda nao esta implementado.",
      "Esta versao mostra apenas a interface, sem iniciar jogos." },
    { "O catalogo real ainda nao esta conectado.",
      "Cards demonstrativos nao representam jogos disponiveis." },
    { "Busca indisponivel ate a conexao com o catalogo.",
      "Nenhuma pesquisa esta sendo enviada a servidores." },
    { "Sua biblioteca ainda nao pode ser sincronizada.",
      "Autenticacao Microsoft/Xbox sera uma etapa separada." },
    { "As configuracoes visuais ja podem ser alteradas.",
      "Feche esta janela e entre em Ajustes para modifica-las." },
    { "O diagnostico da rede local esta disponivel.",
      "A conexao com Xbox Cloud Gaming nao e testada." },
    { "Aplicativo experimental de codigo aberto.",
      "O funcionamento em hardware precisa ser testado." }
};

static void modal(const Cloud360Menu *menu, const Cloud360UiState *ui)
{
    if (menu->message_index >= CLOUD360_SECTION_COUNT)
        return;
    const uint8_t a = cloud360_ui_modal_alpha(ui);
    rect(30, 135, 1220, 490, fade(BACKGROUND, mix_alpha(a, 218)));
    rect(166, 211, 948, 302, fade(SURFACE, a));
    rect(166, 211, 948, 20, fade(AZURE, a));
    cloud360_icon_draw(211, 263, CLOUD360_ICON_INFO, fade(SKY, a));
    label(301, 251, 2, WHITE, a, "RECURSO EM PREPARACAO");
    label(210, 354, 1, WHITE, a, modal_messages[menu->message_index][0]);
    label(210, 390, 1, MUTED, a, modal_messages[menu->message_index][1]);
    label(210, 464, 1, SKY, a, "B  FECHAR");
}

static void footer(const Cloud360Menu *menu, const Cloud360UiState *ui)
{
    const uint8_t a = cloud360_ui_intro_alpha(ui);
    underline(38, 632, 1186, OUTLINE);
    label(42, 659, 1, GREEN, a, "●  PROTOTIPO LOCAL");
    label(260, 659, 1, MUTED, a, "SEM LOGIN OU STREAMING");
    if (menu->settings.show_hints)
        label(675, 659, 1, WHITE, a, "LB/RB  ABAS    A  ABRIR    B  VOLTAR");
    else
        label(945, 659, 1, MUTED, a, "CLOUD360");
}

void cloud360_render(uint32_t address, uint32_t width, uint32_t height,
                     const Cloud360Menu *menu, const Cloud360UiState *ui,
                     const Cloud360Network *network,
                     const Cloud360ProbeSnapshot *probe)
{
    xsdk_gpu_set_target(width, height);
    header(menu, ui);
    if (menu->selected != CLOUD360_SECTION_SETTINGS)
        hero(menu, ui);

    if (menu->selected == CLOUD360_SECTION_HOME ||
        menu->selected == CLOUD360_SECTION_EXPLORE ||
        menu->selected == CLOUD360_SECTION_LIBRARY)
        cards(menu, ui);
    else if (menu->selected == CLOUD360_SECTION_SEARCH)
        search_page(menu, ui);
    else if (menu->selected == CLOUD360_SECTION_SETTINGS)
        settings_page(menu, ui);
    else if (menu->selected == CLOUD360_SECTION_NETWORK)
        network_page(menu, ui, network, probe);
    else
        about_page(menu, ui);

    footer(menu, ui);
    modal(menu, ui);
    xsdk_gpu_resolve(address, width, height, BACKGROUND);
}
