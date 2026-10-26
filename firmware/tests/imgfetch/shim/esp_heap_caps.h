#pragma once
// host shim: heap_caps_* with failure injection (firmware/tests/imgfetch/sim.h)
#include <cstddef>
#define MALLOC_CAP_SPIRAM (1 << 10)
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_DMA (1 << 3)
void *heap_caps_malloc(size_t n, unsigned caps);
void heap_caps_free(void *p);
size_t heap_caps_get_free_size(unsigned caps);
size_t heap_caps_get_largest_free_block(unsigned caps);
