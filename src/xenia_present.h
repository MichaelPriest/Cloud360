#ifndef CLOUD360_XENIA_PRESENT_H
#define CLOUD360_XENIA_PRESENT_H

#include <stdbool.h>
#include <stdint.h>
#include "xsdk/kernel.h"

enum {
    CLOUD360_XENIA_WIDTH = 1280,
    CLOUD360_XENIA_HEIGHT = 720,
    CLOUD360_XENIA_BUFFERS = 2
};

typedef struct {
    volatile uint32_t *pixels[CLOUD360_XENIA_BUFFERS];
    uint32_t addresses[CLOUD360_XENIA_BUFFERS];
    uint32_t fences[CLOUD360_XENIA_BUFFERS];
    uint32_t index;
    uint32_t width;
    uint32_t height;
    uint32_t presented;
    bool opened;
} Cloud360XeniaPresent;

/* Xenia-specific experimental GPU swap. Does not touch the display controller. */
NTSTATUS cloud360_xenia_present_open(Cloud360XeniaPresent *present);
NTSTATUS cloud360_xenia_present_begin(Cloud360XeniaPresent *present, uint32_t *address);
NTSTATUS cloud360_xenia_present_end(Cloud360XeniaPresent *present);
void cloud360_xenia_present_close(Cloud360XeniaPresent *present);

#endif
