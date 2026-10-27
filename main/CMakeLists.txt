idf_component_register(
    SRCS "main.c" "s31_video.c" "s31_platform.cpp" "s31_input_board.cpp" "s31_cpu_selftest.cpp" "s31_flash_storage.c" "s31_remote_input.cpp"
         "../core/main.cpp"
         "../core/main_esp32.cpp"
         "../core/video.cpp"
         "../core/video_esp32.cpp"
         "../core/sys_esp32.cpp"
         "../core/prefs.cpp"
         "../core/prefs_items.cpp"
         "../core/prefs_esp32.cpp"
         "../core/xpram.cpp"
         "../core/xpram_esp32.cpp"
         "../core/timer.cpp"
         "../core/timer_esp32.cpp"
         "../core/mac_clock_esp32.cpp"
         "../core/adb.cpp"
         "../core/disk.cpp"
         "../core/cdrom.cpp"
         "../core/sony.cpp"
         "../core/emul_op.cpp"
         "../core/macos_util.cpp"
         "../core/rom_patches.cpp"
         "../core/rsrc_patches.cpp"
         "../core/slot_rom.cpp"
         "../core/quickdraw_accel.cpp"
         "../core/user_strings.cpp"
         "../core/user_strings_esp32.cpp"
         "../core/audio.cpp"
         "../core/audio_s31.cpp"
         "../core/input_esp32.cpp"
         "../core/hid_descriptor.cpp"
         "../core/touch_overlay.cpp"
         "../core/ether.cpp"
         "../core/ether_dummy.cpp"
         "../core/driver_stubs.cpp"
         "../core/uae_cpu/readcpu.cpp"
         "../core/uae_cpu/newcpu.cpp"
         "../core/uae_cpu/memory.cpp"
         "../core/uae_cpu/basilisk_glue.cpp"
         "../core/uae_cpu/fpu/fpu_ieee.cpp"
         "../core/uae_cpu/generated/cpuemu.cpp"
         "../core/uae_cpu/generated/cpustbl.cpp"
         "../core/uae_cpu/generated/cpudefs.cpp"
    INCLUDE_DIRS "." "../core" "../core/include" "../core/uae_cpu" "../core/uae_cpu/fpu"
    REQUIRES esp_lcd esp_psram esp_driver_gpio esp_driver_uart fatfs wear_levelling esp_timer nvs_flash
             espressif__usb espressif__esp_lcd_touch espressif__esp_codec_dev espressif__arduino-esp32
)

if(S31_CPU_SELFTEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE S31_CPU_SELFTEST=1)
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE USE_RV32_JIT=0 S31_FLASH_BOOT=0)
if(S31_EMU_NO_IO)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE S31_EMU_NO_IO=1)
endif()

# The upstream P4 core logs 32-bit UAE values using both %x and %lx.
# They are the same width on this ILP32 target, but GCC 15 diagnoses the
# type mismatch as an error under the Arduino component's warning flags.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format -Wno-error=unused-label
    -Wno-error=return-type -Wno-error=unused-variable -Wno-error=unused-function
    -Wno-error=unused-but-set-variable -Wno-error=sign-compare)
