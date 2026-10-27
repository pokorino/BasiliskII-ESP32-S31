#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

/* Native 800x480 RGB565 output for the S31's landscape panel. The caller
 * serializes draws with any other user of the LCD and keeps source buffers
 * valid until the operation returns. */
esp_err_t s31_video_blit_rgb565(esp_lcd_panel_handle_t panel,
                                int x, int y, int width, int height,
                                const uint16_t *pixels);

/* Mac indexed source: 1, 2, 4 or 8 bits/pixel, high bits are leftmost.
 * 'src' points at row zero of the guest framebuffer; stride is in bytes. */
esp_err_t s31_video_blit_indexed(esp_lcd_panel_handle_t panel,
                                 int x, int y, int width, int height,
                                 const uint8_t *src, uint32_t stride,
                                 int bits, const uint16_t *palette);
