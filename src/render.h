#ifndef CLOUD360_RENDER_H
#define CLOUD360_RENDER_H

#include <stdint.h>
#include "menu.h"
#include "ui.h"

/* Draws UI to xsdk eDRAM and resolves it into an xsdk_present frame. */
void cloud360_render(uint32_t address, uint32_t width, uint32_t height,
                     const Cloud360Menu *menu, const Cloud360UiState *ui);

#endif
