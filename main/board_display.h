#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_lcd_panel_ops.h"
#ifdef __cplusplus
extern "C" {
#endif
void S31DisplayAttach(esp_lcd_panel_handle_t panel);
int BoardDisplay_Width(void);
int BoardDisplay_Height(void);
bool BoardDisplay_BeginTiles(void);
bool BoardDisplay_EndTiles(void);
bool BoardDisplay_PushTile(int x, int y, int w, int h, const uint16_t *pixels);
bool BoardDisplay_PushIndexedTile(int x, int y, int w, int h, const uint8_t *src,
    uint32_t stride, int bits, int first_pixel, const uint32_t *pairs);
#ifdef __cplusplus
}
#endif
