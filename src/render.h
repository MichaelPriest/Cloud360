#ifndef CLOUD360_RENDER_H
#define CLOUD360_RENDER_H

#include <stdint.h>
#include "menu.h"
#include "network.h"
#include "probe.h"
#include "https.h"
#include "catalog_runtime.h"
#include "oauth.h"
#include "xbl_runtime.h"
#include "session.h"
#include "ui.h"

/* Draws UI to xsdk eDRAM and resolves it into an xsdk_present frame. */
void cloud360_render(uint32_t address, uint32_t width, uint32_t height,
                     const Cloud360Menu *menu, const Cloud360UiState *ui,
                     const Cloud360Network *network,
                     const Cloud360ProbeSnapshot *probe,
                     const Cloud360HttpsSnapshot *https,
                     const Cloud360CatalogView *catalog,
                     const Cloud360OAuthView *auth,
                     const Cloud360XblView *xbl,
                     const Cloud360SessionView *session);

#endif
