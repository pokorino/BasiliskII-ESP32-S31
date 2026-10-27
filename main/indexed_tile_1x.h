#pragma once
#include <stddef.h>
#include <stdint.h>
inline void renderIndexedTile1x(const uint8_t *snapshot, const uint16_t *palette,
                                uint16_t *out, size_t pixel_count)
{
    for (size_t i=0; i<pixel_count; ++i) out[i] = palette[snapshot[i]];
}
