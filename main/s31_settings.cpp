#include "s31_settings.h"
#include "board_sd.h"
#include "board_touch.h"
#include "board_display.h"
#include "chicago_font_data.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <nvs.h>
#include <stdio.h>
#include <string.h>

bool S31BootUsbBegin();
void S31BootUsbPoll();
bool S31BootUsbAnyKeyDown();
uint8_t S31BootUsbTakeKey();
void S31BootUsbFinish();
void S31BootUsbMouse(int *dx, int *dy, uint8_t *buttons);
bool PreallocateCPUHotData();
bool ReserveCompactDispatchIndex();

namespace {
constexpr int W = 800, H = 480, MAX_FILES = 24, MAX_PATH = 256;
constexpr uint16_t BG = 0x0842, CARD = 0x18e4, TEXT = 0xffff, MUTED = 0xad55;
constexpr uint16_t ACCENT = 0x47fb, SELECT = 0x3290;
char rom[MAX_PATH] = "/MAC_ROM.dat", disk[MAX_PATH] = "/mac1.HFV";
int ram = 12;
bool audio = true;
char roms[MAX_FILES][MAX_PATH], disks[MAX_FILES][MAX_PATH];
int rom_count = 0, disk_count = 0, rom_index = 0, disk_index = 0, focus = 0;
uint16_t *canvas = nullptr;
const char *message = "";
int mouse_x = 400, mouse_y = 240;
bool mouse_visible = false;

bool valid_path(const char *p) {
    return p && p[0] == '/' && p[1] && strlen(p) < MAX_PATH && !strchr(p + 1, '/') && !strstr(p, "..");
}
void load() {
    nvs_handle_t n;
    if (nvs_open("s31_settings", NVS_READONLY, &n) != ESP_OK) return;
    int32_t value;
    if (nvs_get_i32(n, "ram_mb", &value) == ESP_OK && (value == 8 || value == 10 || value == 11 || value == 12)) ram = value;
    if (nvs_get_i32(n, "audio", &value) == ESP_OK && (value == 0 || value == 1)) audio = value != 0;
    char path[MAX_PATH]; size_t size = sizeof(path);
    if (nvs_get_str(n, "rom", path, &size) == ESP_OK && valid_path(path)) snprintf(rom, sizeof(rom), "%s", path);
    size = sizeof(path);
    if (nvs_get_str(n, "disk", path, &size) == ESP_OK && valid_path(path)) snprintf(disk, sizeof(disk), "%s", path);
    nvs_close(n);
}
bool save() {
    nvs_handle_t n;
    if (nvs_open("s31_settings", NVS_READWRITE, &n) != ESP_OK) return false;
    esp_err_t err = nvs_set_i32(n, "ram_mb", ram);
    if (err == ESP_OK) err = nvs_set_i32(n, "audio", audio ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_str(n, "rom", rom);
    if (err == ESP_OK) err = nvs_set_str(n, "disk", disk);
    if (err == ESP_OK) err = nvs_commit(n);
    nvs_close(n);
    return err == ESP_OK;
}
void fill(int x, int y, int w, int h, uint16_t c) {
    if (!canvas) return;
    for (int yy = y; yy < y+h && yy < H; ++yy)
        for (int xx = x; xx < x+w && xx < W; ++xx)
            if (xx >= 0 && yy >= 0) canvas[yy * W + xx] = c;
}
void text(int x, int y, const char *str, uint16_t color, int scale = 2, int max_x = 770) {
    for (; *str && x < max_x; ++str) {
        unsigned c = (unsigned char)*str;
        if (c < CHICAGO_FIRST_CP || c > CHICAGO_LAST_CP) c = '?';
        const ChicagoGlyph &g = CHICAGO_GLYPH_TABLE[c - CHICAGO_FIRST_CP];
        int stride = (g.w + 7) / 8;
        for (int py = 0; py < g.h; ++py) for (int px = 0; px < g.w; ++px)
            if (CHICAGO_GLYPH_BLOB[g.offset + py * stride + px / 8] & (0x80 >> (px & 7)))
                fill(x + (g.bbx_x + px) * scale, y + (CHICAGO_ASCENT - g.bbx_y - g.h + py) * scale, scale, scale, color);
        x += g.advance * scale;
    }
}
void present() { if (canvas) BoardDisplay_PushTile(0, 0, W, H, canvas); }
bool suffix(const char *name, const char *ext) {
    size_t a = strlen(name), b = strlen(ext);
    return a >= b && strcasecmp(name + a - b, ext) == 0;
}
void add(char list[][MAX_PATH], int &count, const char *path) {
    if (!valid_path(path) || count >= MAX_FILES) return;
    for (int i=0; i<count; ++i) if (!strcasecmp(list[i], path)) return;
    snprintf(list[count++], MAX_PATH, "%s", path);
}
void scan() {
    rom_count = disk_count = 0;
    add(roms, rom_count, rom); add(disks, disk_count, disk);
    File root = S31SD.open("/");
    if (!root || !root.isDirectory()) return;
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        if (!f.isDirectory()) {
            const char *name = f.name();
            const char *base = strrchr(name, '/'); base = base ? base + 1 : name;
            char path[MAX_PATH]; int n = snprintf(path, sizeof(path), "/%s", base);
            if (n > 0 && n < (int)sizeof(path)) {
                if (suffix(base, ".rom") || suffix(base, ".dat")) add(roms, rom_count, path);
                if (suffix(base, ".hfv") || suffix(base, ".dsk") || suffix(base, ".img")) add(disks, disk_count, path);
            }
        }
        f.close();
    }
    root.close(); rom_index = disk_index = 0;
}
void render() {
    fill(0, 0, W, H, BG); fill(24, 22, 752, 436, CARD); fill(24, 22, 8, 436, ACCENT);
    text(50, 30, "S31  /  SETTINGS", TEXT, 2);
    const char *labels[] = {"MAC RAM", "SOUND", "ROM FILE", "STARTUP DISK", "SAVE AND START"};
    char value[64];
    for (int i=0; i<5; ++i) {
        int y = 91 + i * 62;
        fill(46, y, 708, 52, i == focus ? SELECT : BG);
        text(60, y + 4, labels[i], TEXT, 1);
        if (i == 0) snprintf(value, sizeof(value), "%d MB", ram);
        else if (i == 1) snprintf(value, sizeof(value), "%s", audio ? "ON" : "OFF");
        else if (i == 2) snprintf(value, sizeof(value), "%.42s", rom + 1);
        else if (i == 3) snprintf(value, sizeof(value), "%.42s", disk + 1);
        else snprintf(value, sizeof(value), "ENTER  >");
        text(318, y + 4, value, i == 4 ? ACCENT : TEXT, 1, 720);
        if (i < 4) { text(690, y + 4, ">", ACCENT, 1); text(280, y + 4, "<", ACCENT, 1); }
    }
    text(54, 416, *message ? message : "TOUCH OR MOUSE  /  ARROWS + ENTER", MUTED, 1);
    if (mouse_visible) { fill(mouse_x, mouse_y, 4, 18, TEXT); fill(mouse_x, mouse_y, 13, 4, TEXT); }
    present();
}
void adjust(int direction) {
    const int choices[] = {8,10,11,12};
    if (focus == 0) { int i=0; while (i<3 && choices[i]!=ram) ++i; ram = choices[(i + direction + 4) % 4]; }
    else if (focus == 1) audio = !audio;
    else if (focus == 2 && rom_count) { rom_index = (rom_index + direction + rom_count) % rom_count; snprintf(rom, sizeof(rom), "%s", roms[rom_index]); }
    else if (focus == 3 && disk_count) { disk_index = (disk_index + direction + disk_count) % disk_count; snprintf(disk, sizeof(disk), "%s", disks[disk_index]); }
}
void menu() {
    scan(); focus = 0; message = ""; render();
    bool was_touch = true;
    bool was_mouse = true;
    for (;;) {
        S31BootUsbPoll(); BoardTouch_Update();
        BoardTouchDetail t = BoardTouch_GetDetail();
        uint8_t k = S31BootUsbTakeKey();
        bool redraw = false, start = false;
        int dx, dy; uint8_t buttons;
        S31BootUsbMouse(&dx, &dy, &buttons);
        if (dx || dy) {
            mouse_x = constrain(mouse_x + dx, 0, W - 13);
            mouse_y = constrain(mouse_y + dy, 0, H - 18);
            mouse_visible = true; redraw = true;
        }
        bool mouse_down = (buttons & 1) != 0;
        if (mouse_down && !was_mouse && mouse_x >= 46 && mouse_x < 754 && mouse_y >= 91 && mouse_y < 401) {
            int row = (mouse_y - 91) / 62;
            if (row < 5 && (mouse_y - 91) % 62 < 52) { focus = row; if (row == 4) start = true; else adjust(mouse_x < 318 ? -1 : 1); redraw = true; }
        }
        was_mouse = mouse_down;
        if (t.pressed && !was_touch && t.x >= 46 && t.x < 754 && t.y >= 91 && t.y < 401) {
            int row = (t.y - 91) / 62;
            if (row < 5 && (t.y - 91) % 62 < 52) { focus = row; if (row == 4) start = true; else adjust(t.x < 318 ? -1 : 1); redraw = true; }
        }
        was_touch = t.pressed;
        if (k == 0x52) { focus = (focus + 4) % 5; redraw = true; }
        else if (k == 0x51) { focus = (focus + 1) % 5; redraw = true; }
        else if (k == 0x50 || k == 0x4f) { adjust(k == 0x50 ? -1 : 1); redraw = true; }
        else if (k == 0x28 || k == 0x2c) { if (focus == 4) start = true; else adjust(1); redraw = true; }
        if (start) {
            if (!S31SD.exists(rom) || !S31SD.exists(disk)) { message = "ROM OR DISK NOT FOUND ON SD"; redraw = true; }
            else if (!save()) { message = "SAVE FAILED - TRY AGAIN"; redraw = true; }
            else { fill(0, 0, W, H, BG); text(66, 180, "STARTING MAC...", TEXT, 2); present(); return; }
        }
        if (redraw) render();
        delay(12);
    }
}
}

const char *S31SettingsROM() { return rom; }
const char *S31SettingsDisk() { return disk; }
int S31SettingsRAM() { return ram; }
bool S31SettingsAudio() { return audio; }
void S31SettingsBoot() {
#if !S31_EMU_NO_IO && !S31_FLASH_BOOT && !S31_CPU_SELFTEST
    load();
    // Reserve CPU tables before the USB host fragments internal SRAM.
    if (!PreallocateCPUHotData()) Serial.println("[SETTINGS] CPU table reservation failed");
    if (!ReserveCompactDispatchIndex()) Serial.println("[SETTINGS] compact dispatch unavailable");
    bool touch_ok = BoardTouch_Init();
    bool usb_ok = S31BootUsbBegin();
    // GT1151 reports arrive intermittently even while a finger stays down.
    // Permit short gaps between fresh reports, but require multiple reports
    // spanning the full hold period so one stale sample cannot open settings.
    uint32_t start = millis(), touch_start = 0, touch_last = 0, key_start = 0;
    unsigned touch_reports = 0, hold_reports = 0;
    bool enter = false;
    while (millis() - start < 5000) {
        if (usb_ok) S31BootUsbPoll();
        if (touch_ok) BoardTouch_Update();
        bool touched = touch_ok && BoardTouch_GetDetail().pressed;
        bool key_down = usb_ok && S31BootUsbAnyKeyDown();
        uint32_t now = millis();
        if (touched) {
            ++touch_reports;
            if (!touch_start || now - touch_last > 180) { touch_start = now; hold_reports = 0; }
            touch_last = now; ++hold_reports;
            if (hold_reports >= 3 && now - touch_start >= 600) enter = true;
        } else if (touch_start && now - touch_last > 180) {
            touch_start = touch_last = 0; hold_reports = 0;
        }
        if (key_down) { if (!key_start) key_start = now; if (now - key_start >= 600) enter = true; }
        else key_start = 0;
        if (enter) break;
        delay(12);
    }
    (void)S31BootUsbTakeKey();
    Serial.printf("[SETTINGS] boot touch reports=%u hold=%u entered=%d\n", touch_reports, hold_reports, enter);
    if (enter) {
        canvas = (uint16_t *)heap_caps_malloc(W * H * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (canvas) { Serial.println("[SETTINGS] opened"); menu(); }
        else Serial.println("[SETTINGS] canvas allocation failed; starting Mac");
    } else Serial.println("[SETTINGS] normal boot");
    S31BootUsbFinish();
    if (canvas) { heap_caps_free(canvas); canvas = nullptr; }
#endif
}
