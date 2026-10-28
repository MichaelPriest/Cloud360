/*
 * Host-side exercise of the Xenia-only frontbuffer path.
 * Stubs validate frame ownership and fence ordering, not Xenia GPU output.
 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/thread.h"
#include "../src/xenia_present.h"

typedef struct {
    uint32_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} Cloud360GpuSurface;

static uint32_t allocations;
static uint32_t frees;
static uint32_t swaps;
static uint32_t fences;
static uint32_t waits;
static uint32_t sleeps;
static uint32_t allocation_failure_at;
static bool simulate_timeout;

void xsdk_log_write(const char *format, ...)
{
    (void)format;
}

uint32_t xsdk_gpu_resolve_bytes(uint32_t width, uint32_t height)
{
    assert(width == 1280 && height == 720);
    return width * height * 4;
}

volatile uint32_t *xsdk_gpu_alloc(uint32_t size_bytes)
{
    ++allocations;
    if (allocation_failure_at && allocations == allocation_failure_at)
        return NULL;
    return (volatile uint32_t *)calloc(size_bytes, 1);
}

void xsdk_gpu_free(volatile uint32_t *pixels)
{
    ++frees;
    free((void *)pixels);
}

uint32_t xsdk_gpu_address(volatile const uint32_t *pixels)
{
    assert(pixels != NULL);
    return (uint32_t)(uintptr_t)pixels;
}

bool xsdk_gpu_swap(volatile const uint32_t *pixels, uint32_t width,
                    uint32_t height, Cloud360GpuSurface *shown)
{
    assert(pixels != NULL && shown != NULL);
    assert(width == 1280 && height == 720);
    ++swaps;
    return false;
}

uint32_t xsdk_gpu_fence(void)
{
    return ++fences;
}

NTSTATUS xsdk_gpu_wait(uint32_t fence, uint32_t timeout_ms)
{
    assert(fence != 0 && timeout_ms > 0);
    ++waits;
    return simulate_timeout ? STATUS_IO_TIMEOUT : STATUS_SUCCESS;
}

NTSTATUS xsdk_thread_sleep(uint32_t delay_ms)
{
    assert(delay_ms == 16);
    ++sleeps;
    return STATUS_SUCCESS;
}

static void reset(void)
{
    allocations = frees = swaps = fences = waits = sleeps = 0;
    allocation_failure_at = 0;
    simulate_timeout = false;
}

static void test_buffers_and_swap(void)
{
    reset();
    Cloud360XeniaPresent display;
    assert(NT_SUCCESS(cloud360_xenia_present_open(&display)));
    assert(display.opened && allocations == 2);
    assert(display.addresses[0] != display.addresses[1]);
    uint32_t address = 0;
    assert(NT_SUCCESS(cloud360_xenia_present_begin(&display, &address)));
    assert(address == display.addresses[0]);
    assert(NT_SUCCESS(cloud360_xenia_present_end(&display)));
    assert(swaps == 1 && fences == 1 && display.index == 1);

    assert(NT_SUCCESS(cloud360_xenia_present_begin(&display, &address)));
    assert(address == display.addresses[1]);
    assert(NT_SUCCESS(cloud360_xenia_present_end(&display)));
    assert(swaps == 2 && fences == 2 && display.index == 0);

    assert(NT_SUCCESS(cloud360_xenia_present_begin(&display, &address)));
    assert(waits == 1 && address == display.addresses[0]);
    assert(display.presented == 2);
    cloud360_xenia_present_close(&display);
    assert(frees == 2 && sleeps == 2);
}

static void test_partial_failure(void)
{
    reset();
    allocation_failure_at = 2;
    Cloud360XeniaPresent display;
    assert(cloud360_xenia_present_open(&display) == STATUS_NO_MEMORY);
    assert(!display.opened && allocations == 2 && frees == 1);
    assert(cloud360_xenia_present_begin(&display, NULL) == STATUS_INVALID_PARAMETER);
}

static void test_timeout_and_safe_cleanup(void)
{
    reset();
    Cloud360XeniaPresent display;
    assert(NT_SUCCESS(cloud360_xenia_present_open(&display)));
    uint32_t address = 0;
    assert(NT_SUCCESS(cloud360_xenia_present_begin(&display, &address)));
    assert(NT_SUCCESS(cloud360_xenia_present_end(&display)));
    assert(NT_SUCCESS(cloud360_xenia_present_begin(&display, &address)));
    assert(NT_SUCCESS(cloud360_xenia_present_end(&display)));
    simulate_timeout = true;
    assert(cloud360_xenia_present_begin(&display, &address) == STATUS_IO_TIMEOUT);
    cloud360_xenia_present_close(&display);
    assert(frees == 0); /* Cannot free in-flight GPU buffers. */
    simulate_timeout = false;
}

int main(void)
{
    test_buffers_and_swap();
    test_partial_failure();
    test_timeout_and_safe_cleanup();
    puts("Cloud360 Xenia frontbuffer & fence tests: PASS");
    return 0;
}
