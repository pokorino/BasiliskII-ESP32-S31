#if S31_FLASH_BOOT
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "wear_levelling.h"

static wl_handle_t flash_wl = WL_INVALID_HANDLE;

esp_err_t s31_flash_storage_mount(void)
{
    const esp_vfs_fat_mount_config_t config = {
        .format_if_mount_failed = false,
        .max_files = 12,
        .allocation_unit_size = 4096,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl("/sd", "storage", &config, &flash_wl);
    if (err != ESP_OK) return err;
    // Retain the VFS path used by the emulator; its backing medium is flash.
    const char *names[] = {"/sd/Q650.ROM", "/sd/SYS70.DSK"};
    const size_t sizes[] = {1048576, 1474560};
    for (unsigned i = 0; i < 2; ++i) {
        struct stat st;
        if (stat(names[i], &st) != 0 || (size_t)st.st_size != sizes[i]) return ESP_ERR_INVALID_SIZE;
        ESP_LOGI("flash_storage", "%s: %ld bytes", names[i], (long)st.st_size);
    }
    const char *probe = "/sd/FLASH.TST";
    const char expected[] = "flash-ok";
    FILE *f = fopen(probe, "wb");
    if (!f) return ESP_FAIL;
    size_t written = fwrite(expected, 1, sizeof(expected), f);
    int closed = fclose(f);
    if (written != sizeof(expected) || closed != 0) return ESP_FAIL;
    char actual[sizeof(expected)] = {0};
    f = fopen(probe, "rb");
    if (!f) return ESP_FAIL;
    size_t got = fread(actual, 1, sizeof(actual), f);
    fclose(f);
    remove(probe);
    if (got != sizeof(actual) || memcmp(expected, actual, sizeof(actual)) != 0) return ESP_FAIL;
    ESP_LOGI("flash_storage", "Flash FAT read/write verified; SDMMC unused");
    return ESP_OK;
}
#endif
