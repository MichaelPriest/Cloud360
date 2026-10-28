#include "ui.h"

enum { EASE_ONE = 1024, AMBIENT_PERIOD_MS = 4800 };

static uint32_t clamp_add(uint32_t elapsed, uint32_t dt, uint32_t limit)
{
    if (elapsed >= limit || dt >= limit - elapsed)
        return limit;
    return elapsed + dt;
}

/* Ease-out cubic in Q10 fixed point: no floating point or frame-rate assumptions. */
static uint32_t eased(uint32_t elapsed, uint32_t duration)
{
    if (elapsed >= duration)
        return EASE_ONE;
    const uint32_t t = elapsed * EASE_ONE / duration;
    const uint32_t remain = EASE_ONE - t;
    const uint32_t squared = remain * remain / EASE_ONE;
    const uint32_t cubed = squared * remain / EASE_ONE;
    return EASE_ONE - cubed;
}

void cloud360_ui_init(Cloud360UiState *ui)
{
    ui->cursor_y = CLOUD360_UI_NAV_TOP;
    ui->cursor_start_y = CLOUD360_UI_NAV_TOP;
    ui->cursor_target_y = CLOUD360_UI_NAV_TOP;
    ui->cursor_ms = CLOUD360_UI_SELECTION_MS;
    ui->content_ms = 0;
    ui->intro_ms = 0;
    ui->modal_ms = 0;
    ui->ambient_ms = 0;
    ui->previous_selected = 0;
    ui->previous_message = CLOUD360_MENU_NO_MESSAGE;
}

void cloud360_ui_step(Cloud360UiState *ui, const Cloud360Menu *menu, uint32_t dt_ms)
{
    /* A paused frame should not cause an abrupt visual jump. */
    if (dt_ms > 64)
        dt_ms = 64;

    if (menu->selected != ui->previous_selected) {
        ui->cursor_start_y = ui->cursor_y;
        ui->cursor_target_y = CLOUD360_UI_NAV_TOP + (int32_t)menu->selected * CLOUD360_UI_NAV_STEP;
        ui->cursor_ms = 0;
        ui->content_ms = 0;
        ui->previous_selected = menu->selected;
    }

    if (menu->message_index != ui->previous_message) {
        ui->modal_ms = 0;
        ui->previous_message = menu->message_index;
    }

    ui->cursor_ms = clamp_add(ui->cursor_ms, dt_ms, CLOUD360_UI_SELECTION_MS);
    ui->content_ms = clamp_add(ui->content_ms, dt_ms, CLOUD360_UI_SELECTION_MS);
    ui->intro_ms = clamp_add(ui->intro_ms, dt_ms, CLOUD360_UI_INTRO_MS);

    if (menu->message_index < CLOUD360_MENU_COUNT)
        ui->modal_ms = clamp_add(ui->modal_ms, dt_ms, CLOUD360_UI_MODAL_MS);

    ui->ambient_ms = (ui->ambient_ms + dt_ms) % AMBIENT_PERIOD_MS;

    const uint32_t t = eased(ui->cursor_ms, CLOUD360_UI_SELECTION_MS);
    const int32_t delta = ui->cursor_target_y - ui->cursor_start_y;
    ui->cursor_y = ui->cursor_start_y + (int32_t)((int64_t)delta * t / EASE_ONE);
}

uint8_t cloud360_ui_content_alpha(const Cloud360UiState *ui)
{
    return (uint8_t)(eased(ui->content_ms, CLOUD360_UI_SELECTION_MS) * 255 / EASE_ONE);
}

uint8_t cloud360_ui_intro_alpha(const Cloud360UiState *ui)
{
    return (uint8_t)(eased(ui->intro_ms, CLOUD360_UI_INTRO_MS) * 255 / EASE_ONE);
}

uint8_t cloud360_ui_modal_alpha(const Cloud360UiState *ui)
{
    return (uint8_t)(eased(ui->modal_ms, CLOUD360_UI_MODAL_MS) * 255 / EASE_ONE);
}

uint8_t cloud360_ui_ambient_alpha(const Cloud360UiState *ui)
{
    uint32_t triangle = ui->ambient_ms;
    if (triangle > AMBIENT_PERIOD_MS / 2)
        triangle = AMBIENT_PERIOD_MS - triangle;
    return (uint8_t)(18 + triangle * 22 / (AMBIENT_PERIOD_MS / 2));
}

int32_t cloud360_ui_content_offset(const Cloud360UiState *ui)
{
    return (int32_t)((EASE_ONE - eased(ui->content_ms, CLOUD360_UI_SELECTION_MS)) * 16 / EASE_ONE);
}
