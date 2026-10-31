#include <Arduino.h>
#include <FS.h>
#include <vfs_api.h>
#include "board_sd.h"
#include "board_display.h"
#include "boot_gui.h"
#include "input.h"
#include "automation.h"
#include "touch_overlay.h"
#include "esp_system.h"
#include "s31_settings.h"

static fs::FSImplPtr sd_impl(new VFSImpl());
fs::FS S31SD(sd_impl);
void S31FilesystemInit() { sd_impl->mountpoint("/sd"); }

static esp_lcd_panel_handle_t lcd;
void S31DisplayAttach(esp_lcd_panel_handle_t panel) { lcd = panel; }
int BoardDisplay_Width() { return 800; }
int BoardDisplay_Height() { return 480; }
bool BoardDisplay_BeginTiles() { return lcd != nullptr; }
bool BoardDisplay_EndTiles() { return true; }
bool BoardDisplay_PushTile(int x, int y, int w, int h, const uint16_t *pixels) {
    if (!lcd || !pixels || x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > 800 || y + h > 480) return false;
    return esp_lcd_panel_draw_bitmap(lcd, x, y, x + w, y + h, pixels) == ESP_OK;
}
bool BoardDisplay_PushIndexedTile(int, int, int, int, const uint8_t *, uint32_t, int, int, const uint32_t *) {
    // The 1x renderer expands indexed tiles before calling PushTile.
    return false;
}

// Fixed boot settings for the first board validation.
const char *BootGUI_GetDiskPath() {
#if S31_FLASH_BOOT
    return "/SYS70.DSK";
#else
    return S31SettingsDisk();
#endif
}
const char *BootGUI_GetCDROMPath() { return ""; }
const char *BootGUI_GetExtFSPath() { return ""; }
uint32_t BootGUI_GetRAMSize() { return (uint32_t)S31SettingsRAM() * 1024u * 1024u; }
int BootGUI_GetRAMSizeMB() { return S31SettingsRAM(); }
bool BootGUI_GetAudioEnabled() {
#if S31_EMU_NO_IO
    return false;
#else
    return S31SettingsAudio();
#endif
}
bool BootGUI_GetBootFromCD() { return false; }
bool BootGUI_IsWiFiConnected() { return false; }
uint32_t BootGUI_GetWiFiIP() { return 0; }
const char *BootGUI_GetWiFiSSID() { return ""; }
const char *BootGUI_GetWiFiPassword() { return ""; }
bool BootGUI_GetWiFiAutoConnect() { return false; }
int BootGUI_GetRotation() { return 0; }
bool boot_control_pending = false;
void BootGUI_ControlPoll() {}
void BootGUI_ControlSetPhase(const char *phase) { printf("[S31] phase: %s\n", phase); }
bool BootGUI_RestartPending() { return false; }
void BootGUI_Restart() { esp_restart(); }

bool AutomationInit() { return false; }
void AutomationExit() {}
bool AutomationSerialCaptureActive() { return false; }
void PlayStartupSound() {}

extern void basilisk_setup();
static void emulator_task(void *) {
    initArduino();
    Serial.begin(115200);
    S31FilesystemInit();
    S31SettingsBoot();
    basilisk_setup();
    vTaskDelete(nullptr);
}
extern "C" bool s31_start_emulator() {
    return xTaskCreatePinnedToCore(emulator_task, "basilisk68k", 32768,
        nullptr, 1, nullptr, 1) == pdPASS;
}
