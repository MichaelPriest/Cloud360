#include "ui.h"

enum { EASE_ONE = 1024, AMBIENT_PERIOD_MS = 4800 };

static uint32_t clamp_add(uint32_t elapsed, uint32_t dt, uint32_t limit)
{
    if (elapsed >= limit || dt >= limit - elapsed)
        return limit;
    return elapsed + dt;
}

/* Cubic ease-out, fixed point Q10; no floating-point work per frame. */
static uint32_t ease(uint32_t elapsed, uint32_t duration)
{
    if (elapsed >= duration)
        return EASE_ONE;
    const uint32_t t = elapsed * EASE_ONE / duration;
    const uint32_t back = EASE_ONE - t;
    return EASE_ONE - (back * back / EASE_ONE) * back / EASE_ONE;
}

static int32_t lerp(int32_t first, int32_t last, uint32_t t)
{
    return first + (int32_t)((int64_t)(last - first) * t / EASE_ONE);
}

void cloud360_ui_init(Cloud360UiState *ui)
{
    ui->tab_x = CLOUD360_UI_TAB_X;
    ui->tab_start_x = CLOUD360_UI_TAB_X;
    ui->tab_target_x = CLOUD360_UI_TAB_X;
    ui->setting_y = CLOUD360_UI_SETTINGS_Y;
    ui->setting_start_y = CLOUD360_UI_SETTINGS_Y;
    ui->setting_target_y = CLOUD360_UI_SETTINGS_Y;
    ui->tab_ms = CLOUD360_UI_TAB_MS;
    ui->setting_ms = CLOUD360_UI_TAB_MS;
    ui->page_ms = 0;
    ui->intro_ms = 0;
    ui->modal_ms = 0;
    ui->focus_ms = 0;
    ui->ambient_ms = 0;
    ui->previous_section = CLOUD360_SECTION_HOME;
    ui->previous_setting = 0;
    ui->previous_card = 0;
    ui->previous_message = CLOUD360_NO_MESSAGE;
    ui->previous_focus = false;
}

void cloud360_ui_step(Cloud360UiState *ui, const Cloud360Menu *menu, uint32_t dt_ms)
{
    if (dt_ms > 64)
        dt_ms = 64;

    if (menu->selected != ui->previous_section) {
        ui->tab_start_x = ui->tab_x;
        ui->tab_target_x = CLOUD360_UI_TAB_X + (int32_t)menu->selected * CLOUD360_UI_TAB_STEP;
        ui->tab_ms = 0;
        ui->page_ms = 0;
        ui->previous_section = menu->selected;
    }
    if (menu->setting_index != ui->previous_setting) {
        ui->setting_start_y = ui->setting_y;
        ui->setting_target_y = CLOUD360_UI_SETTINGS_Y + (int32_t)menu->setting_index * CLOUD360_UI_SETTINGS_STEP;
        ui->setting_ms = 0;
        ui->previous_setting = menu->setting_index;
    }
    if (menu->card_index != ui->previous_card || menu->content_focus != ui->previous_focus) {
        ui->focus_ms = 0;
        ui->previous_card = menu->card_index;
        ui->previous_focus = menu->content_focus;
    }
    if (menu->message_index != ui->previous_message) {
        ui->modal_ms = 0;
        ui->previous_message = menu->message_index;
    }

    const bool animate = menu->settings.animations;
    ui->tab_ms = animate ? clamp_add(ui->tab_ms, dt_ms, CLOUD360_UI_TAB_MS) : CLOUD360_UI_TAB_MS;
    ui->setting_ms = animate ? clamp_add(ui->setting_ms, dt_ms, CLOUD360_UI_TAB_MS) : CLOUD360_UI_TAB_MS;
    ui->page_ms = animate ? clamp_add(ui->page_ms, dt_ms, CLOUD360_UI_PAGE_MS) : CLOUD360_UI_PAGE_MS;
    ui->intro_ms = animate ? clamp_add(ui->intro_ms, dt_ms, CLOUD360_UI_INTRO_MS) : CLOUD360_UI_INTRO_MS;
    ui->focus_ms = animate ? clamp_add(ui->focus_ms, dt_ms, CLOUD360_UI_TAB_MS) : CLOUD360_UI_TAB_MS;

    if (menu->message_index != CLOUD360_NO_MESSAGE)
        ui->modal_ms = animate ? clamp_add(ui->modal_ms, dt_ms, CLOUD360_UI_MODAL_MS) : CLOUD360_UI_MODAL_MS;

    if (menu->settings.ambient_effects)
        ui->ambient_ms = (ui->ambient_ms + dt_ms) % AMBIENT_PERIOD_MS;
    else
        ui->ambient_ms = 0;

    ui->tab_x = lerp(ui->tab_start_x, ui->tab_target_x, ease(ui->tab_ms, CLOUD360_UI_TAB_MS));
    ui->setting_y = lerp(ui->setting_start_y, ui->setting_target_y, ease(ui->setting_ms, CLOUD360_UI_TAB_MS));
}

uint8_t cloud360_ui_page_alpha(const Cloud360UiState *ui)
{
    return (uint8_t)(255 * ease(ui->page_ms, CLOUD360_UI_PAGE_MS) / EASE_ONE);
}

uint8_t cloud360_ui_intro_alpha(const Cloud360UiState *ui)
{
    return (uint8_t)(255 * ease(ui->intro_ms, CLOUD360_UI_INTRO_MS) / EASE_ONE);
}

uint8_t cloud360_ui_modal_alpha(const Cloud360UiState *ui)
{
    return (uint8_t)(255 * ease(ui->modal_ms, CLOUD360_UI_MODAL_MS) / EASE_ONE);
}

uint8_t cloud360_ui_focus_alpha(const Cloud360UiState *ui)
{
    return (uint8_t)(255 * ease(ui->focus_ms, CLOUD360_UI_TAB_MS) / EASE_ONE);
}

uint8_t cloud360_ui_ambient_alpha(const Cloud360UiState *ui)
{
    uint32_t tick = ui->ambient_ms;
    if (tick > AMBIENT_PERIOD_MS / 2)
        tick = AMBIENT_PERIOD_MS - tick;
    return (uint8_t)(15 + tick * 22 / (AMBIENT_PERIOD_MS / 2));
}

int32_t cloud360_ui_page_offset(const Cloud360UiState *ui)
{
    return (int32_t)((EASE_ONE - ease(ui->page_ms, CLOUD360_UI_PAGE_MS)) * 18 / EASE_ONE);
}
