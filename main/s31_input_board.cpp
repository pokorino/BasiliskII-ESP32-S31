#include "board.h"
#include "board_touch.h"
#include "board_keyboard.h"
#include "bsp/touch.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"

static esp_lcd_touch_handle_t touch;
static BoardTouchMulti points;

bool BoardTouch_Init(void)
{
    if (touch) return true;
    const bsp_touch_config_t config = {};
    esp_err_t err = bsp_touch_new(&config, &touch);
    if (err != ESP_OK) {
        ESP_LOGW("S31-touch", "GT1151 init failed: %s", esp_err_to_name(err));
        touch = nullptr;
        return false;
    }
    ESP_LOGI("S31-touch", "GT1151 ready, native 800x480 coordinates");
    return true;
}

void BoardTouch_Update(void)
{
    points = {};
    if (!touch || esp_lcd_touch_read_data(touch) != ESP_OK) return;
    esp_lcd_touch_point_data_t data[BOARD_TOUCH_MAX_POINTS] = {};
    uint8_t count = 0;
    if (esp_lcd_touch_get_data(touch, data, &count, BOARD_TOUCH_MAX_POINTS) != ESP_OK) return;
    for (uint8_t i = 0; i < count && i < BOARD_TOUCH_MAX_POINTS; ++i) {
        if (data[i].x >= 800 || data[i].y >= 480) continue;
        points.p[points.count++] = {true, data[i].x, data[i].y, data[i].track_id};
    }
}

void BoardTouch_GetMulti(BoardTouchMulti *out) { if (out) *out = points; }
BoardTouchDetail BoardTouch_GetDetail(void) {
    return points.count ? points.p[0] : BoardTouchDetail{false, 0, 0, -1};
}
bool Board_Init(void) { return BoardTouch_Init(); }
void Board_Update(void) { BoardTouch_Update(); }

// The Korvo-1 has no Tab5 matrix keyboard. USB keyboards use the HID path.
bool BoardKeyboard_Init(void) { return true; }
bool BoardKeyboard_Poll(BoardKeyboardEvent *) { return false; }
bool BoardKeyboard_IsConnected(void) { return false; }
void BoardKeyboard_Exit(void) {}
