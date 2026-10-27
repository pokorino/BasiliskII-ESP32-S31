#include <array>
#include <algorithm>
#include <cstring>
#define OVERLAY_HOST_TEST
#include "../core/touch_overlay.cpp"

static int notifications = 0;
static TouchOverlayMode expected_mode;
extern "C" void VideoMarkTilesDirtyRect(int, int, int w, int h)
{
    assert(w > 0 && h > 0);
    // Damage notification must only become observable after the new snapshot.
    assert(TouchOverlay_GetMode() == expected_mode);
    ++notifications;
}

int main()
{
    TouchOverlay_Init(800, 480, 800, 480);
    assert(!TouchOverlay_IsVisible());
    BoardTouchMulti finger = {};
    finger.count = 1;
    finger.p[0] = {true, 400, 200, 1};
    test_ms = 1000;
    TouchOverlay_Update(&finger);
    assert(host_mouse_x == 400 && host_mouse_y == 200 && host_mouse_down_count == 0);
    test_ms += 20;
    TouchOverlay_Update(&finger);
    assert(host_mouse_down_count == 1);
    finger.p[0].x = 460; finger.p[0].y = 230;
    TouchOverlay_Update(&finger);
    assert(host_mouse_x == 460 && host_mouse_y == 230);
    finger.count = 0;
    TouchOverlay_Update(&finger);
    assert(host_mouse_up_count == 1);
    // At native 1x scale every font bitmap bit must remain visible.
    std::array<uint16_t, 64 * 64> glyph_pixels{};
    composite_glyph(glyph_pixels.data(), 0, 0, 64, 64, 10, 32, 'I', 1, 0xffff);
    const ChicagoGlyph &glyph = CHICAGO_GLYPH_TABLE['I' - CHICAGO_FIRST_CP];
    unsigned expected_pixels = 0;
    for (unsigned y = 0; y < glyph.h; ++y)
        for (unsigned x = 0; x < glyph.w; ++x)
            expected_pixels += !!(CHICAGO_GLYPH_BLOB[glyph.offset + y * ((glyph.w + 7) / 8) + x / 8] & (0x80 >> (x % 8)));
    assert(std::count(glyph_pixels.begin(), glyph_pixels.end(), 0xffff) == expected_pixels);

    expected_mode = TOUCH_OVERLAY_KEYBOARD;
    {
        OverlayInputGuard guard;
        toggle_mode(expected_mode);
    }
    assert(notifications == 1);
    TouchOverlay_BeginFrame();
    assert(s_frame.mode == TOUCH_OVERLAY_KEYBOARD);
    const auto keyboard = s_frame;
    expected_mode = TOUCH_OVERLAY_GAMING;
    {
        OverlayInputGuard guard;
        toggle_mode(expected_mode);
    }
    assert(notifications == 2 && s_frame.mode == TOUCH_OVERLAY_KEYBOARD);
    // All tiles of the old batch still see exactly the same keyboard.
    assert(memcmp(&keyboard, &s_frame, sizeof(keyboard)) == 0);
    TouchOverlay_BeginFrame();
    assert(s_frame.mode == TOUCH_OVERLAY_GAMING && s_frame.count == s_game_count);
    {
        OverlayInputGuard guard;
        press_key_index(0);
    }
    assert(!s_frame.keys[0].held && s_published.keys[0].held);
    TouchOverlay_BeginFrame();
    assert(s_frame.keys[0].held);
    // Guard the compositing buffer while exercising every panel tile.
    std::array<uint16_t, 32 * 40 + 2> tile;
    for (int y = 0; y < 480; y += 40) for (int x = 0; x < 800; x += 32) {
        tile.fill(0x1234); tile.front() = 0xdead; tile.back() = 0xbeef;
        TouchOverlay_CompositeTile(x, y, 32, 40, tile.data() + 1);
        assert(tile.front() == 0xdead && tile.back() == 0xbeef);
    }
    expected_mode = TOUCH_OVERLAY_NONE;
    TouchOverlay_Shutdown();
    assert(!TouchOverlay_IsVisible());
    TouchOverlay_BeginFrame();
    assert(s_frame.mode == TOUCH_OVERLAY_NONE);
    puts("PASS: overlay publication ordering, immutable batches, key state and tile bounds");
}
