#ifndef CLOUD360_UI_H
#define CLOUD360_UI_H

#include <stdint.h>
#include "menu.h"

/* Fixed timestep-independent animation values. No graphics/console dependency. */
enum {
    CLOUD360_UI_SELECTION_MS = 240,
    CLOUD360_UI_INTRO_MS = 440,
    CLOUD360_UI_MODAL_MS = 220,
    CLOUD360_UI_NAV_TOP = 204,
    CLOUD360_UI_NAV_STEP = 82
};

typedef struct {
    int32_t cursor_y;
    int32_t cursor_start_y;
    int32_t cursor_target_y;
    uint32_t cursor_ms;
    uint32_t content_ms;
    uint32_t intro_ms;
    uint32_t modal_ms;
    uint32_t ambient_ms;
    uint32_t previous_selected;
    uint32_t previous_message;
} Cloud360UiState;

void cloud360_ui_init(Cloud360UiState *ui);
void cloud360_ui_step(Cloud360UiState *ui, const Cloud360Menu *menu, uint32_t dt_ms);
uint8_t cloud360_ui_content_alpha(const Cloud360UiState *ui);
uint8_t cloud360_ui_intro_alpha(const Cloud360UiState *ui);
uint8_t cloud360_ui_modal_alpha(const Cloud360UiState *ui);
uint8_t cloud360_ui_ambient_alpha(const Cloud360UiState *ui);
int32_t cloud360_ui_content_offset(const Cloud360UiState *ui);

#endif
