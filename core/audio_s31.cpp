/*
 *  audio_s31.cpp - Audio support for the S31 Korvo-1
 *  Adapted from the P4 codec backend for the Korvo-1 ES8389.
 *
 *  BasiliskII ESP32 Port
 *
 *  Uses the S31 Korvo-1 BSP's esp_codec_dev-based ES8389 speaker path:
 *     bsp_audio_init()                - I2S channel setup
 *     bsp_audio_codec_speaker_init()  - ES8389 codec and amplifier enable
 *     esp_codec_dev_open/write/close  - sample submission
 *
 *  This file mirrors the public interface of src/basilisk/audio_esp32.cpp
 *  (the Tab5 version) so the rest of the codebase is unaware of the board.
 *  The Apple Mixer pump, format conversion, and volume math are the same;
 *  only the final sink is different.
 */

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "audio_guest_memory.h"
#include "s31_audio_pcm.h"
#include "main.h"
#include "prefs.h"
#include "audio.h"
#include "audio_defs.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "bsp/esp32_s31_korvo_1.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#define DEBUG 0
#include "debug.h"

/* ----------------------------------------------------------------------- */
/* Configuration                                                           */
/* ----------------------------------------------------------------------- */

#define AUDIO_TASK_STACK_SIZE  4096
#define AUDIO_TASK_PRIORITY    2

/* Pin audio on the core opposite to the emulation loop. */
#if defined(CONFIG_ARDUINO_RUNNING_CORE)
#define EMULATION_TASK_CORE CONFIG_ARDUINO_RUNNING_CORE
#elif defined(ARDUINO_RUNNING_CORE)
#define EMULATION_TASK_CORE ARDUINO_RUNNING_CORE
#else
#define EMULATION_TASK_CORE 1
#endif

#if defined(portNUM_PROCESSORS) && (portNUM_PROCESSORS > 1)
#define AUDIO_TASK_CORE ((EMULATION_TASK_CORE == 0) ? 1 : 0)
#else
#define AUDIO_TASK_CORE EMULATION_TASK_CORE
#endif

#define AUDIO_SAMPLE_RATE      22050
#define AUDIO_BUFFER_FRAMES    1024
#define AUDIO_CHANNELS         2
#define AUDIO_SAMPLE_SIZE      16
#define AUDIO_BYTES_PER_FRAME  (AUDIO_CHANNELS * (AUDIO_SAMPLE_SIZE / 8))
#define AUDIO_BUFFER_SIZE      (AUDIO_BUFFER_FRAMES * AUDIO_BYTES_PER_FRAME)

#define MAC_MAX_VOLUME 0x0100

/* ----------------------------------------------------------------------- */
/* State                                                                   */
/* ----------------------------------------------------------------------- */

static int audio_sample_rate_index   = 0;
static int audio_sample_size_index   = 0;
static int audio_channel_count_index = 0;

static TaskHandle_t     audio_task_handle  = NULL;
static volatile bool    audio_task_running = false;
static SemaphoreHandle_t audio_irq_done_sem = NULL;
static SemaphoreHandle_t audio_stopped_sem = NULL;

static int16_t *audio_mix_buf = NULL;
static esp_codec_dev_handle_t codec_dev = NULL;
static bool speaker_initialized = false;

static int  main_volume    = MAC_MAX_VOLUME / 2;
static int  speaker_volume = MAC_MAX_VOLUME / 2;
static bool main_mute      = false;
static bool speaker_mute   = false;

static const char *TAG = "audio_s31";

/* ----------------------------------------------------------------------- */
/* Helpers                                                                 */
/* ----------------------------------------------------------------------- */

static void set_audio_status_format(void)
{
    AudioStatus.sample_rate = audio_sample_rates[audio_sample_rate_index];
    AudioStatus.sample_size = audio_sample_sizes[audio_sample_size_index];
    AudioStatus.channels    = audio_channel_counts[audio_channel_count_index];
}

/* Map Mac 8.8 fixed point volume to 0..100 for esp_codec_dev. */
static int get_effective_volume_percent(void)
{
    if (main_mute || speaker_mute) return 0;
    uint32_t combined = (main_volume * speaker_volume) / MAC_MAX_VOLUME;
    uint32_t pct = (combined * 100) / MAC_MAX_VOLUME;
    if (pct > 100) pct = 100;
    return (int)pct;
}

// The Korvo-1 BSP owns the ES8389 I2C/I2S and amplifier pin setup.
static bool init_speaker(void)
{
    codec_dev = bsp_audio_codec_speaker_init();
    if (!codec_dev) return false;
    esp_codec_dev_sample_info_t format = {};
    format.bits_per_sample = AUDIO_SAMPLE_SIZE;
    format.channel = AUDIO_CHANNELS;
    format.sample_rate = AUDIO_SAMPLE_RATE;
    if (esp_codec_dev_open(codec_dev, &format) != ESP_CODEC_DEV_OK) {
        esp_codec_dev_delete(codec_dev);
        codec_dev = nullptr;
        return false;
    }
    if (esp_codec_dev_set_out_vol(codec_dev, get_effective_volume_percent()) != ESP_CODEC_DEV_OK ||
        esp_codec_dev_set_out_mute(codec_dev, false) != ESP_CODEC_DEV_OK) {
        esp_codec_dev_close(codec_dev);
        esp_codec_dev_delete(codec_dev);
        codec_dev = nullptr;
        return false;
    }
    speaker_initialized = true;
    Serial.printf("[AUDIO] ES8389 ready: %d Hz stereo 16-bit\n", AUDIO_SAMPLE_RATE);
    return true;
}

static void stop_speaker(void)
{
    if (!speaker_initialized) return;
    if (codec_dev) {
        esp_codec_dev_close(codec_dev);
        esp_codec_dev_delete(codec_dev);
        codec_dev = NULL;
    }
    speaker_initialized = false;
    Serial.println("[AUDIO] Speaker stopped");
}

/* ----------------------------------------------------------------------- */
/* Audio task                                                              */
/* ----------------------------------------------------------------------- */

static void audioTask(void *param)
{
    (void)param;
    Serial.printf("[AUDIO] Audio task started on Core %d\n", xPortGetCoreID());

    const TickType_t active_poll_interval = pdMS_TO_TICKS(2);
    const TickType_t idle_poll_interval   = pdMS_TO_TICKS(20);

    int last_num_sources = -1;

    while (audio_task_running) {
        if (AudioStatus.num_sources != last_num_sources) {
            Serial.printf("[AUDIO] num_sources %d -> %d\n",
                          last_num_sources, AudioStatus.num_sources);
            last_num_sources = AudioStatus.num_sources;
        }

        if (AudioStatus.num_sources > 0 &&
            audio_open &&
            audio_irq_done_sem != NULL &&
            speaker_initialized) {

            /* Drop any stale completion signal before issuing a request. */
            while (xSemaphoreTake(audio_irq_done_sem, 0) == pdTRUE) {}

            D(bug("[AUDIO] Triggering audio interrupt\n"));
            SetInterruptFlag(INTFLAG_AUDIO);
            TriggerInterrupt();

            if (xSemaphoreTake(audio_irq_done_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (!AudioGuestRAMRange(audio_data, adatStreamInfo + 4)) continue;
                uint32_t apple_stream_info = ReadMacInt32(audio_data + adatStreamInfo);
                WriteMacInt32(audio_data + adatStreamInfo, 0);

                if (apple_stream_info && audio_mix_buf != NULL &&
                    AudioGuestRAMRange(apple_stream_info, scd_buffer + 4)) {
                    const uint32_t sample_count    = ReadMacInt32(apple_stream_info + scd_sampleCount);
                    const uint32_t src_channels    = ReadMacInt16(apple_stream_info + scd_numChannels);
                    const uint32_t src_sample_size = ReadMacInt16(apple_stream_info + scd_sampleSize);
                    const uint32_t src_buffer_mac  = ReadMacInt32(apple_stream_info + scd_buffer);

                    bool format_ok = true;
                    if (sample_count == 0 || sample_count > AUDIO_BUFFER_FRAMES) format_ok = false;
                    if ((src_channels != 1 && src_channels != 2) ||
                        (src_sample_size != 8 && src_sample_size != 16)) format_ok = false;

                    if (format_ok) {
                        const uint32_t source_bytes = sample_count * src_channels * (src_sample_size / 8);
                        const uint8_t *src = AudioGuestSamples(src_buffer_mac, source_bytes);
                        if (src != NULL) {
                            const int out_samples = (int)sample_count * AUDIO_CHANNELS;
                            if ((int)(out_samples * sizeof(int16_t)) <= AUDIO_BUFFER_SIZE) {
                                if (!S31ConvertPCM(src, source_bytes, sample_count,
                                                   src_channels, src_sample_size,
                                                   audio_mix_buf, AUDIO_BUFFER_SIZE / sizeof(int16_t))) continue;

                                /* Submit to codec. esp_codec_dev_write blocks
                                 * until DMA has consumed the buffer. */
                                esp_codec_dev_write(codec_dev, audio_mix_buf,
                                                    out_samples * sizeof(int16_t));
                            }
                        }
                    }
                }
            } else {
                D(bug("[AUDIO] Timeout waiting for AudioInterrupt\n"));
            }
        }

        vTaskDelay((AudioStatus.num_sources > 0) ? active_poll_interval : idle_poll_interval);
    }

    xSemaphoreGive(audio_stopped_sem);
    Serial.println("[AUDIO] Audio task exiting");
    vTaskDelete(NULL);
}

/* ----------------------------------------------------------------------- */
/* Lifecycle                                                               */
/* ----------------------------------------------------------------------- */

static bool open_audio(void)
{
    audio_sample_rates.clear();
    audio_sample_sizes.clear();
    audio_channel_counts.clear();
    audio_sample_rates.push_back(AUDIO_SAMPLE_RATE << 16);
    audio_sample_sizes.push_back(AUDIO_SAMPLE_SIZE);
    audio_channel_counts.push_back(AUDIO_CHANNELS);
    audio_sample_rate_index   = 0;
    audio_sample_size_index   = 0;
    audio_channel_count_index = 0;

    audio_frames_per_block = AUDIO_BUFFER_FRAMES;

    if (audio_mix_buf == NULL) {
        audio_mix_buf = (int16_t *)heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (audio_mix_buf == NULL) {
            Serial.println("[AUDIO] ERROR: failed to allocate PSRAM mix buffer");
            return false;
        }
        memset(audio_mix_buf, 0, AUDIO_BUFFER_SIZE);
        Serial.printf("[AUDIO] Allocated %d byte mix buffer in PSRAM\n", AUDIO_BUFFER_SIZE);
    }

    if (!init_speaker()) return false;

    set_audio_status_format();
    audio_open = true;
    return true;
}

static void close_audio(void)
{
    stop_speaker();
    if (audio_mix_buf) {
        heap_caps_free(audio_mix_buf);
        audio_mix_buf = NULL;
    }
    audio_open = false;
}

/* ----------------------------------------------------------------------- */
/* Public API                                                              */
/* ----------------------------------------------------------------------- */

void AudioInit(void)
{
    Serial.println("[AUDIO] Initializing audio subsystem (S31 Korvo-1)...");

    AudioStatus.sample_rate = AUDIO_SAMPLE_RATE << 16;
    AudioStatus.sample_size = AUDIO_SAMPLE_SIZE;
    AudioStatus.channels    = AUDIO_CHANNELS;
    AudioStatus.mixer       = 0;
    AudioStatus.num_sources = 0;
    audio_component_flags   = cmpWantsRegisterMessage | kStereoOut | k16BitOut;

    if (PrefsFindBool("nosound")) {
        Serial.println("[AUDIO] Sound disabled in preferences");
        return;
    }

    audio_stopped_sem = xSemaphoreCreateBinary();
    if (!audio_stopped_sem) return;
    audio_irq_done_sem = xSemaphoreCreateBinary();
    if (audio_irq_done_sem == NULL) {
        Serial.println("[AUDIO] ERROR: failed to create semaphore");
        return;
    }

    if (!open_audio()) {
        Serial.println("[AUDIO] failed to open audio device");
        return;
    }

    audio_task_running = true;
    BaseType_t r = xTaskCreatePinnedToCore(
        audioTask, "AudioTask", AUDIO_TASK_STACK_SIZE, NULL,
        AUDIO_TASK_PRIORITY, &audio_task_handle, AUDIO_TASK_CORE);
    if (r != pdPASS) {
        Serial.println("[AUDIO] ERROR: failed to create audio task");
        audio_task_running = false;
        close_audio();
        return;
    }

    Serial.printf("[AUDIO] Audio task created on Core %d (emulation core: %d)\n",
                  AUDIO_TASK_CORE, EMULATION_TASK_CORE);
}

void AudioExit(void)
{
    Serial.println("[AUDIO] Shutting down audio subsystem...");
    if (audio_task_handle) {
        audio_task_running = false;
        if (xSemaphoreTake(audio_stopped_sem, pdMS_TO_TICKS(2000)) != pdTRUE) {
            Serial.println("[AUDIO] Task did not stop; buffers retained");
            return;
        }
        audio_task_handle = nullptr;
    }
    close_audio();
    if (audio_irq_done_sem) {
        vSemaphoreDelete(audio_irq_done_sem);
        audio_irq_done_sem = NULL;
    }
    if (audio_stopped_sem) { vSemaphoreDelete(audio_stopped_sem); audio_stopped_sem = nullptr; }
    Serial.println("[AUDIO] Audio subsystem shut down");
}

void audio_enter_stream(void) { /* handled implicitly */ }

void audio_exit_stream(void)
{
    /* Best-effort: no direct analog to M5.Speaker.stop() - esp_codec_dev
     * draining is handled naturally by the blocking write model. */
}

void AudioInterrupt(void)
{
    if (AudioStatus.mixer) {
        M68kRegisters r;
        WriteMacInt32(audio_data + adatStreamInfo, 0);
        r.a[0] = audio_data + adatStreamInfo;
        r.a[1] = AudioStatus.mixer;
        Execute68k(audio_data + adatGetSourceData, &r);
        if (r.d[0] != 0) {
            WriteMacInt32(audio_data + adatStreamInfo, 0);
        }
    } else {
        WriteMacInt32(audio_data + adatStreamInfo, 0);
    }
    if (audio_irq_done_sem) xSemaphoreGive(audio_irq_done_sem);
}

bool audio_set_sample_rate(int index)
{
    if (index < 0 || index >= (int)audio_sample_rates.size()) return false;
    audio_sample_rate_index = index;
    set_audio_status_format();
    return true;
}

bool audio_set_sample_size(int index)
{
    if (index < 0 || index >= (int)audio_sample_sizes.size()) return false;
    audio_sample_size_index = index;
    set_audio_status_format();
    return true;
}

bool audio_set_channels(int index)
{
    if (index < 0 || index >= (int)audio_channel_counts.size()) return false;
    audio_channel_count_index = index;
    set_audio_status_format();
    return true;
}

bool  audio_get_main_mute(void)          { return main_mute; }
uint32 audio_get_main_volume(void)       { return main_volume; }
bool  audio_get_speaker_mute(void)       { return speaker_mute; }
uint32 audio_get_speaker_volume(void)    { return speaker_volume; }

void audio_set_main_mute(bool mute)
{
    main_mute = mute;
    if (codec_dev) esp_codec_dev_set_out_mute(codec_dev, mute || speaker_mute);
}

void audio_set_main_volume(uint32 vol)
{
    main_volume = vol;
    if (codec_dev) esp_codec_dev_set_out_vol(codec_dev, (float)get_effective_volume_percent());
}

void audio_set_speaker_mute(bool mute)
{
    speaker_mute = mute;
    if (codec_dev) esp_codec_dev_set_out_mute(codec_dev, mute || main_mute);
}

void audio_set_speaker_volume(uint32 vol)
{
    speaker_volume = vol;
    if (codec_dev) esp_codec_dev_set_out_vol(codec_dev, (float)get_effective_volume_percent());
}

/* AudioReset() is defined in src/basilisk/audio.cpp; don't duplicate it. */
