#include "s31_video.h"
#include <stdbool.h>
#include <stddef.h>

#define PANEL_WIDTH 800
#define PANEL_HEIGHT 480
#define TILE_WIDTH 32
#define TILE_HEIGHT 40

static bool tile_valid(int x, int y, int width, int height)
{
    return x >= 0 && y >= 0 && width > 0 && height > 0 &&
           width <= TILE_WIDTH && height <= TILE_HEIGHT &&
           x + width <= PANEL_WIDTH && y + height <= PANEL_HEIGHT;
}

esp_err_t s31_video_blit_rgb565(esp_lcd_panel_handle_t panel,
                                int x, int y, int width, int height,
                                const uint16_t *pixels)
{
    if (!panel || !pixels || !tile_valid(x, y, width, height))
        return ESP_ERR_INVALID_ARG;
    return esp_lcd_panel_draw_bitmap(panel, x, y, x + width, y + height,
                                     pixels);
}

esp_err_t s31_video_blit_indexed(esp_lcd_panel_handle_t panel,
                                 int x, int y, int width, int height,
                                 const uint8_t *src, uint32_t stride,
                                 int bits, const uint16_t *palette)
{
    if (!panel || !src || !palette || !tile_valid(x, y, width, height) ||
        (bits != 1 && bits != 2 && bits != 4 && bits != 8) ||
        stride < (uint32_t)((x + width) * bits + 7) / 8)
        return ESP_ERR_INVALID_ARG;

    uint16_t tile[TILE_WIDTH * TILE_HEIGHT];
    const unsigned mask = (1u << bits) - 1u;
    for (int row = 0; row < height; ++row) {
        const uint8_t *line = src + (size_t)(y + row) * stride;
        for (int col = 0; col < width; ++col) {
            const unsigned bit = (unsigned)(x + col) * (unsigned)bits;
            const unsigned index = (line[bit >> 3] >> (8u - bits - (bit & 7u))) & mask;
            tile[row * width + col] = palette[index];
        }
    }
    return esp_lcd_panel_draw_bitmap(panel, x, y, x + width, y + height, tile);
}
