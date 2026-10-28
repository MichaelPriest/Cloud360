#ifndef CLOUD360_UI_H
#define CLOUD360_UI_H

#include <stdint.h>
#include "menu.h"

enum {
    CLOUD360_UI_TAB_X = 252,
    CLOUD360_UI_TAB_STEP = 123,
    CLOUD360_UI_SETTINGS_Y = 228,
    CLOUD360_UI_SETTINGS_STEP = 72,
    CLOUD360_UI_TAB_MS = 200,
    CLOUD360_UI_PAGE_MS = 240,
    CLOUD360_UI_INTRO_MS = 420,
    CLOUD360_UI_MODAL_MS = 200
};

typedef struct {
    int32_t tab_x;
    int32_t tab_start_x;
    int32_t tab_target_x;
    int32_t setting_y;
    int32_t setting_start_y;
    int32_t setting_target_y;
    uint32_t tab_ms;
    uint32_t setting_ms;
    uint32_t page_ms;
    uint32_t intro_ms;
    uint32_t modal_ms;
    uint32_t focus_ms;
    uint32_t ambient_ms;
    uint32_t previous_section;
    uint32_t previous_setting;
    uint32_t previous_card;
    uint32_t previous_message;
    bool previous_focus;
} Cloud360UiState;

void cloud360_ui_init(Cloud360UiState *ui);
void cloud360_ui_step(Cloud360UiState *ui, const Cloud360Menu *menu, uint32_t dt_ms);

uint8_t cloud360_ui_page_alpha(const Cloud360UiState *ui);
uint8_t cloud360_ui_intro_alpha(const Cloud360UiState *ui);
uint8_t cloud360_ui_modal_alpha(const Cloud360UiState *ui);
uint8_t cloud360_ui_focus_alpha(const Cloud360UiState *ui);
uint8_t cloud360_ui_ambient_alpha(const Cloud360UiState *ui);
int32_t cloud360_ui_page_offset(const Cloud360UiState *ui);

#endif
