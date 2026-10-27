#include <stdint.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp/esp32_s31_korvo_1.h"
#include "bsp/display.h"
#include "board_display.h"
#define PANEL_PIXELS (800u * 480u)
static const char *TAG = "basilisk_s31";
static uint16_t *panel_fb;
extern bool s31_start_emulator(void);

static esp_err_t init_display(void)
{
    bsp_display_config_t config = {0};
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(bsp_display_new(&config, &panel, &io), TAG, "LCD init");
    if (!panel) return ESP_FAIL;
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(panel, 1,
                                                         (void **)&panel_fb),
                        TAG, "LCD framebuffer");
    if (!panel_fb) return ESP_FAIL;
    /* RGB panel is native 800x480 landscape. Show a test card until the
     * emulator's first frame arrives. Keep this buffer off the app stack. */
    static uint16_t line[800];
    for (size_t y = 0; y < 480; ++y) {
        const uint16_t color = y < 160 ? 0x001fu : y < 320 ? 0x07e0u : 0xf800u;
        for (size_t x = 0; x < 800; ++x) line[x] = color;
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel, 0, (int)y,
                                                     800, (int)y + 1, line),
                            TAG, "LCD test row");
    }
    S31DisplayAttach(panel);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "LCD enable");
    ESP_LOGI(TAG, "LCD framebuffer ready: %u bytes", (unsigned)(PANEL_PIXELS * 2));
    return ESP_OK;
}

void app_main(void)
{
#if S31_CPU_SELFTEST
    extern bool s31_start_cpu_selftest(void);
    if (!s31_start_cpu_selftest()) ESP_LOGE(TAG, "CPU selftest task allocation failed");
    return;
#endif
    ESP_LOGI(TAG, "ESP32-S31 Korvo-1 BasiliskII");
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "NVS init: %s", esp_err_to_name(err)); return; }
    err = init_display();
    if (err != ESP_OK) { ESP_LOGE(TAG, "LCD init: %s", esp_err_to_name(err)); return; }
#if S31_FLASH_BOOT
    extern esp_err_t s31_flash_storage_mount(void);
    err = s31_flash_storage_mount();
    if (err != ESP_OK) { ESP_LOGE(TAG, "Flash mount: %s", esp_err_to_name(err)); return; }
#else
    err = bsp_sdcard_mount();
    if (err != ESP_OK) { ESP_LOGE(TAG, "SD mount: %s", esp_err_to_name(err)); return; }
#endif
    ESP_LOGI(TAG, "Free PSRAM: %u bytes", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (!s31_start_emulator()) ESP_LOGE(TAG, "Failed to start emulator task");
}
