# Basilisk II for ESP32-S31-Korvo-1

**Experimental, interpreter-only classic Macintosh emulator port.** Developed by pokorino LABO.

This release is intended for the Espressif **ESP32-S31-Korvo-1** with the attached **800 × 480 RGB LCD and touch panel (GT1151 driver; controller reported GT1158)**, 16 MiB flash and 16 MiB PSRAM. It is based on Basilisk II and an ESP32-P4 port; see [credits and licenses](THIRD_PARTY.md).

## What has been tested

The release binary was built from this source and flashed to the board with the packaged Windows tool. On that board, Mac OS 8.1 booted from a microSD disk image to the Finder with 12 MiB of emulated RAM. Touch input, opening the settings screen, changing RAM and saving the configuration were exercised; the settings text was reported readable. A predecessor build also exercised USB keyboard input, USB mouse movement and clicks, opening settings using Enter, and Mac OS shutdown. The release changes the user-interface font to DejaVu Sans for clear redistribution terms.

Audio codec initialization appeared in the log, but audible output was not tested. USB hubs, other displays, long-term stability, additional ROM/disk combinations, and selecting alternative ROMs/disks in the settings menu have not been verified. These observations are not a complete compatibility claim.

The application has no wireless configuration and does not deliberately start Wi-Fi, Bluetooth, or IEEE 802.15.4 radios. Some ESP-IDF dependencies include wireless-related components; this is not an RF certification or a radio-emission measurement.

## Quick start: Windows x64

[日本語の導入手順](docs/WINDOWS-JA.md)

1. Download `BasiliskII-S31-Windows.zip` from the [Releases page](https://github.com/pokorino/BasiliskII-ESP32-S31/releases) and extract the **entire** ZIP. It includes the official Espressif esptool, so Python and ESP-IDF are not required for flashing. Windows ARM and 32-bit Windows are not supported by this package.
2. Connect the board's USB serial interface to the PC. If Windows does not show a COM port, install the correct USB-to-serial driver for the adapter; the tested setup enumerated as a Silicon Labs CP210x. The correct port may differ on another computer.
3. Double-click `flash.cmd`, enter the board's `COM` port (for example `COM3`), and type `YES`. Wait for all hash checks and `Flash verified`. Do not disconnect power during flashing. The script restarts the board automatically.
4. Prepare a FAT32 microSD card as described below. With power off, insert it and then power on the board. The screen remains black during the settings-shortcut window and while the Mac starts.

The script writes **bootloader.bin at 0x2000**, **partition-table.bin at 0x8000**, and **basiliskii-s31.bin at 0x10000**, using DIO, 80 MHz, 16 MB flash. It does not erase the entire flash or write the microSD card. It does replace the flash partition table; data in an existing flash layout may be inaccessible or overwritten. Existing NVS settings are not intentionally erased. Back up any important flash contents first.

The distribution also contains `esptool.exe`, `flash.ps1`, `flash.cmd`, `LICENSE-GPL-2.0.txt`, `THIRD_PARTY.txt`, and `SHA256SUMS.txt`. If the script cannot connect, check the selected COM port, the USB cable and driver, and the board's boot mode. Do not use the ROM or OS files from untrusted sources.

## ROM and disk images: supplied by the user

No Macintosh ROM, Mac OS image, disk image or Apple software is included. Supply your own legally obtained compatible ROM and bootable classic Mac OS disk image. Back up the image: it is opened **read/write**, and the host disk code can update HFS volume metadata when it detects an unclean volume.

For a fresh configuration, copy these files to the **root of the FAT32 microSD card**:

| Filename | Contents |
| --- | --- |
| `MAC_ROM.dat` | Compatible Macintosh ROM |
| `mac1.HFV` | Bootable Mac OS disk image |

Existing saved settings can select different filenames. Settings scans only the card root: ROM extensions `.rom` and `.dat`; disk extensions `.hfv`, `.dsk`, and `.img` (case-insensitive). Up to 24 candidates of each kind are listed, including the current selection. The filename is not proof that a ROM or disk is compatible. Use disk images under 2 GiB; the backend uses 32-bit file offsets. Only one startup disk is selected by this UI.

The ROM used in the developer's test was identified by their Windows Basilisk II as Performa 580/588 (`064DC91D` ROM checksum). The developer owns a Performa 588. A matching short checksum alone does not prove two ROM files are byte-identical. This build sets the emulator model ID to **14 (Quadra 900)** and CPU type to 68040; the configured model ID is not the physical model of the ROM donor. Compatibility with other ROMs and Mac OS versions is not assured.

## Settings

During the first five seconds after startup, hold a USB keyboard key or touch the screen for approximately 0.6 seconds to open the settings screen. Keep the key held until the menu appears. On the tested board, touching the screen **before** turning on the power switch did not register reliably; switch on first, wait briefly, then touch and hold. Otherwise the Mac boots automatically.

Use touch or USB mouse on a row, or the keyboard arrows to select a row and change its value. `Enter` or `Space` changes the focused value; on **SAVE AND START** it saves and starts the Mac. The menu offers:

- `MAC RAM`: 8, 10, 11 or 12 MiB. The fresh default is 12 MiB; previously saved NVS settings take precedence.
- `SOUND`: on or off. Real sound output remains unverified.
- `ROM FILE` and `STARTUP DISK`: cycle through eligible files in the microSD root.
- `SAVE AND START`: check that both files exist, save settings to NVS, and start.

Settings access by screen, RAM selection, saving, and the revised font were checked on the release build. The Enter shortcut was checked on the predecessor build; alternate-file booting remains unverified. If the card fails to mount at boot, the settings screen is not available.

After Mac OS **Shut Down**, the application stops and the LCD becomes black. The S31 board remains powered; turn its power switch off or reset it to start again.

## Build from source

Use **ESP-IDF 6.1** and the dependencies pinned in `dependencies.lock`: Espressif `esp32_s31_korvo_1_noglib` 1.0.1 and Arduino-ESP32 4.0.0-rc1. The development environment used ESP-IDF commit `fff9895c82d744c7237be8847347bdd1b07c6643` and esptool 5.6.0. Other versions are not verified. Activate the ESP-IDF environment and run:

```sh
idf.py set-target esp32s31
idf.py build
idf.py -p COM3 flash monitor
```

Replace `COM3` with the board's actual port. ROM and disk images belong on the microSD card; the public build does not package them. Do not use the private flash-image validation configuration in this repository. Source builds can differ byte-for-byte from the prebuilt release due to toolchain and build metadata.

## Browser flashing

A browser installer is **not included or tested**. A possible future implementation is described in [docs/WEB-FLASHING.md](docs/WEB-FLASHING.md); use the Windows package for this release.

## Credits and licensing

Thanks to Christian Bauer and Basilisk II contributors; Bernd Schmidt and UAE contributors; and [amcchord/M5Tab-Macintosh](https://github.com/amcchord/M5Tab-Macintosh), the ESP32-P4 port used as a starting point. Source notices are preserved. See [LICENSE](LICENSE) and [THIRD_PARTY.md](THIRD_PARTY.md) for project, font, tool, and dependency licenses.
