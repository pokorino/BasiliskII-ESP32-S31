#pragma once
#include <stddef.h>
#include <stdint.h>

// Classic Mac PCM is unsigned 8-bit or big-endian signed 16-bit. ES8389
// receives native signed 16-bit stereo. Validate complete buffers first.
inline bool S31ConvertPCM(const uint8_t *src, size_t src_bytes, size_t frames,
                          unsigned channels, unsigned bits,
                          int16_t *dst, size_t dst_samples)
{
    if (!src || !dst || !frames || (channels != 1 && channels != 2) ||
        (bits != 8 && bits != 16)) return false;
    const size_t bytes_per_frame = channels * (bits / 8);
    if (frames > src_bytes / bytes_per_frame || frames > dst_samples / 2) return false;
    for (size_t frame = 0; frame < frames; ++frame) {
        for (unsigned channel = 0; channel < 2; ++channel) {
            const size_t index = (frame * channels + (channels == 1 ? 0 : channel)) * (bits / 8);
            dst[frame * 2 + channel] = bits == 8
                ? (int16_t)(((int)src[index] - 128) * 256)
                : (int16_t)(((uint16_t)src[index] << 8) | src[index + 1]);
        }
    }
    return true;
}
