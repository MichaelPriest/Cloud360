/*
 * Cloud360 — first XEX bring-up prototype.
 *
 * This is not yet an Xbox Cloud Gaming client. It is a minimal scaffold
 * intended to explore the native application lifecycle on Xbox 360.
 * API compatibility must be verified against the selected xsdk revision.
 */

#include <stdint.h>
#include <stdbool.h>

#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/splash.h"

enum {
    SPLASH_MS = 1500,
    BUFFER_COUNT = 2,
    SWAP_INTERVAL = 1,
    BEGIN_TIMEOUT_MS = 5000,
    CLEAR_ARGB = 0xFF101820
};

static void run_title(uint32_t width, uint32_t height)
{
    (void)height;

    xsdk_log_write("Cloud360: running at %ux%u", (unsigned)width, (unsigned)height);
    xsdk_log_write("Cloud360: press Back to return to Aurora");

    for (;;) {
        INPUT_STATE input = { 0 };

        if (XamInputGetState(0, 0, &input) == ERROR_SUCCESS &&
            (input.gamepad.buttons & INPUT_GAMEPAD_BACK) != 0) {
            xsdk_log_write("Cloud360: Back pressed; exiting");
            return;
        }

        uint32_t framebuffer_address = 0;
        const NTSTATUS status = xsdk_present_begin(&framebuffer_address, BEGIN_TIMEOUT_MS);
        if (!NT_SUCCESS(status)) {
            xsdk_log_write("Cloud360: xsdk_present_begin failed: 0x%08X",
                           (unsigned)status);
            return;
        }

        /* Initial milestone: render a solid background only. */
        xsdk_gpu_resolve(framebuffer_address, width, height, CLEAR_ARGB);
        xsdk_present_end();
    }
}

int main(void)
{
    xsdk_log_write("Cloud360: startup");

    const NTSTATUS splash_status = xsdk_splash_show(SPLASH_MS);
    if (!NT_SUCCESS(splash_status)) {
        xsdk_log_write("Cloud360: splash failed: 0x%08X",
                       (unsigned)splash_status);
    }

    NTSTATUS status = xsdk_gpu_open();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: xsdk_gpu_open failed: 0x%08X",
                       (unsigned)status);
        return 1;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    status = xsdk_present_open(XSDK_DISPLAY_720P, BUFFER_COUNT,
                               SWAP_INTERVAL, &width, &height);
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: xsdk_present_open failed: 0x%08X",
                       (unsigned)status);
        (void)xsdk_gpu_close();
        return 1;
    }

    run_title(width, height);

    xsdk_present_close();
    status = xsdk_gpu_close();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("Cloud360: xsdk_gpu_close failed: 0x%08X",
                       (unsigned)status);
    }

    xsdk_log_write("Cloud360: shutdown complete");
    return 0;
}
