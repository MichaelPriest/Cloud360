/*
 * Experimental frontbuffer swap for Xenia Canary.
 *
 * The current XSDK display/present layer requires display controller registers
 * that Xenia leaves absent (STATUS_NOT_SUPPORTED). Xenia does, however, accept
 * xboxkrnl VdSwap and uses its PM4_XE_SWAP command to present a front buffer.
 *
 * We use XSDK's existing ring/swap implementation without opening its display
 * controller. xsdk_gpu_swap is an INTERNAL XSDK symbol: GpuSurface mirrors
 * xenosworks/xsdk/src/gpu_private.h at the pinned SDK revision in build.yml.
 * This backend is deliberately isolated from the real-console cloud360.xex.
 *
 * Tested as source and via a stubbed host backend; Xenia runtime is NOT
 * automatically available in GitHub CI, so user validation is still required.
 */
#include "xenia_present.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "xsdk/gpu.h"
#include "xsdk/log.h"
#include "xsdk/thread.h"

enum {
    FRAME_TIMEOUT_MS = 4000,
    FRAME_DELAY_MS = 16,
    LOG_EVERY_FRAMES = 120
};

/* Keep this ABI aligned with xsdk/src/gpu_private.h :: GpuSurface. */
typedef struct {
    uint32_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} Cloud360GpuSurface;

/* XSDK internal function (non-public API), not an emulator patch. */
extern bool xsdk_gpu_swap(volatile const uint32_t *front_buffer, uint32_t width,
                          uint32_t height, Cloud360GpuSurface *shown);

NTSTATUS cloud360_xenia_present_open(Cloud360XeniaPresent *present)
{
    if (!present)
        return STATUS_INVALID_PARAMETER;

    *present = (Cloud360XeniaPresent){ 0 };
    present->width = CLOUD360_XENIA_WIDTH;
    present->height = CLOUD360_XENIA_HEIGHT;
    const uint32_t size = xsdk_gpu_resolve_bytes(present->width, present->height);

    for (uint32_t i = 0; i < CLOUD360_XENIA_BUFFERS; ++i) {
        present->pixels[i] = xsdk_gpu_alloc(size);
        if (!present->pixels[i]) {
            cloud360_xenia_present_close(present);
            xsdk_log_write("XENIA: frontbuffer %u allocation failed", (unsigned)i);
            return STATUS_NO_MEMORY;
        }
        present->addresses[i] = xsdk_gpu_address(present->pixels[i]);
    }

    present->opened = true;
    xsdk_log_write("XENIA: VdSwap backend ready %ux%u, %u buffers",
                   (unsigned)present->width, (unsigned)present->height,
                   (unsigned)CLOUD360_XENIA_BUFFERS);
    return STATUS_SUCCESS;
}

NTSTATUS cloud360_xenia_present_begin(Cloud360XeniaPresent *present, uint32_t *address)
{
    if (!present || !present->opened || !address)
        return STATUS_INVALID_PARAMETER;

    const uint32_t slot = present->index;
    if (present->fences[slot]) {
        const NTSTATUS waited = xsdk_gpu_wait(present->fences[slot], FRAME_TIMEOUT_MS);
        if (!NT_SUCCESS(waited)) {
            xsdk_log_write("XENIA: frontbuffer %u fence timed out: 0x%08X",
                           (unsigned)slot, (unsigned)waited);
            return waited;
        }
        present->fences[slot] = 0;
    }

    *address = present->addresses[slot];
    return STATUS_SUCCESS;
}

NTSTATUS cloud360_xenia_present_end(Cloud360XeniaPresent *present)
{
    if (!present || !present->opened)
        return STATUS_INVALID_PARAMETER;

    const uint32_t slot = present->index;

    /* VdSwap submits the current XSDK PM4 render stream and appends XE_SWAP,
       which Xenia uses to copy the resolved frame into its host output. The
       bool return indicates XAM system overlay, NOT failure/success. */
    Cloud360GpuSurface system_surface = { 0 };
    (void)xsdk_gpu_swap(present->pixels[slot], present->width,
                        present->height, &system_surface);
    present->fences[slot] = xsdk_gpu_fence();
    present->index = (slot + 1) % CLOUD360_XENIA_BUFFERS;
    ++present->presented;

    if (present->presented == 1 || present->presented % LOG_EVERY_FRAMES == 0)
        xsdk_log_write("XENIA: submitted frame %u via VdSwap",
                       (unsigned)present->presented);

    (void)xsdk_thread_sleep(FRAME_DELAY_MS);
    return STATUS_SUCCESS;
}

void cloud360_xenia_present_close(Cloud360XeniaPresent *present)
{
    if (!present)
        return;

    for (uint32_t i = 0; i < CLOUD360_XENIA_BUFFERS; ++i) {
        if (!present->pixels[i])
            continue;

        if (present->fences[i]) {
            const NTSTATUS waited = xsdk_gpu_wait(present->fences[i], FRAME_TIMEOUT_MS);
            if (!NT_SUCCESS(waited)) {
                /* GPU may still access the memory. Leak rather than free an
                   in-flight frontbuffer, then let title shutdown handle it. */
                xsdk_log_write("XENIA: preserving busy frontbuffer %u (0x%08X)",
                               (unsigned)i, (unsigned)waited);
                present->pixels[i] = NULL;
                continue;
            }
        }
        xsdk_gpu_free(present->pixels[i]);
        present->pixels[i] = NULL;
        present->fences[i] = 0;
    }
    present->opened = false;
}
