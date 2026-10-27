#if S31_EMU_NO_IO
#include "sysdeps.h"
#include "adb.h"
#include "driver/uart.h"
#include "esp_log.h"

// UART1 is bridged to a loopback TCP socket by esp-emu. Eight-byte packets:
// 'B','2', type, flags, x/code little-endian u16, y little-endian u16.
static bool keys[128];
static unsigned buttons;
static TaskHandle_t remote_task;

static void release_all() {
    for (unsigned i = 0; i < 128; ++i) if (keys[i]) { ADBKeyUp(i); keys[i] = false; }
    for (unsigned i = 0; i < 3; ++i) if (buttons & (1u << i)) ADBMouseUp(i);
    buttons = 0;
}

static bool apply(const uint8_t *p) {
    unsigned x = p[4] | (p[5] << 8), y = p[6] | (p[7] << 8);
    if (p[2] == 1) {
        if (x >= 800 || y >= 480 || (p[3] & ~7u)) return false;
        ADBSetRelMouseMode(false);
        ADBMouseMoved(x, y);
        bool button_changed = buttons != p[3];
        // Absolute motion is committed by the guest VBL before its ADB
        // button handler reads the cursor. Keep the edge at that position.
        if (button_changed) vTaskDelay(pdMS_TO_TICKS(40));
        for (unsigned i = 0; i < 3; ++i) {
            bool down = (p[3] & (1u << i)) != 0;
            if (down != ((buttons & (1u << i)) != 0)) {
                if (down) ADBMouseDown(i); else ADBMouseUp(i);
            }
        }
        buttons = p[3];
        // Give the guest an interrupt/poll boundary between down and up.
        // TCP can deliver an entire double click in a single UART RX batch.
        if (button_changed) vTaskDelay(pdMS_TO_TICKS(100));
    } else if (p[2] == 2) {
        if (x >= 128 || p[3] > 1) return false;
        bool down = p[3] != 0;
        if (down != keys[x]) {
            if (down) ADBKeyDown(x); else ADBKeyUp(x);
            keys[x] = down;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } else if (p[2] == 3) {
        release_all();
    } else if (p[2] == 4) {
        // Keepalive: preserve held keys/buttons while the browser has focus.
    } else return false;
    return true;
}

static void remote_loop(void *) {
    uint8_t packet[8];
    unsigned used = 0;
    TickType_t last_packet = xTaskGetTickCount();
    for (;;) {
        uint8_t c;
        if (uart_read_bytes(UART_NUM_1, &c, 1, pdMS_TO_TICKS(20)) == 1) {
            if (used == 0 && c != 'B') continue;
            if (used == 1 && c != '2') { used = c == 'B' ? 1 : 0; continue; }
            packet[used++] = c;
            if (used == sizeof(packet)) {
                uint8_t ack = apply(packet) ? 0xac : 0xee;
                uart_write_bytes(UART_NUM_1, &ack, 1);
                last_packet = xTaskGetTickCount();
                used = 0;
            }
        } else if (used && xTaskGetTickCount() - last_packet > pdMS_TO_TICKS(1000)) used = 0;
        // Recover from a browser crash or lost key-up; the viewer sends keepalives.
        if (xTaskGetTickCount() - last_packet > pdMS_TO_TICKS(5000)) {
            release_all();
            last_packet = xTaskGetTickCount();
        }
    }
}

bool S31RemoteInputStart() {
    if (remote_task) return true;
    uart_config_t config = {};
    config.baud_rate = 115200;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    if (uart_param_config(UART_NUM_1, &config) != ESP_OK ||
        uart_driver_install(UART_NUM_1, 2048, 0, 0, nullptr, 0) != ESP_OK) return false;
    if (xTaskCreatePinnedToCore(remote_loop, "remote_adb", 4096, nullptr, 3, &remote_task, 0) != pdPASS) {
        uart_driver_delete(UART_NUM_1); return false;
    }
    ESP_LOGI("remote_input", "UART1 ADB input ready");
    return true;
}
#endif
