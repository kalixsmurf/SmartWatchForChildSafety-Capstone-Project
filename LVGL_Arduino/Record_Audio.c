/* I2S Digital Microphone Recording Example */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_pdm.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "format_wav.h"


#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#define NUM_CHANNELS        (1)              // Mono recording
#define SD_MOUNT_POINT      "/sdcard"
#define SAMPLE_SIZE         (CONFIG_EXAMPLE_BIT_SAMPLE * 1024)
#define BYTE_RATE           (CONFIG_EXAMPLE_SAMPLE_RATE * (CONFIG_EXAMPLE_BIT_SAMPLE / 8)) * NUM_CHANNELS

// Global variables
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
sdmmc_card_t *card= NULL;
i2s_chan_handle_t rx_handle = NULL;
static int16_t i2s_readraw_buff[SAMPLE_SIZE];
size_t bytes_read;
const int WAVE_HEADER_SIZE = 44;

static const char *TAG = "pdm_rec_example";
void sd_card_initialization(void)
{
    // SD Card Initialization
    //sdmmc_card_t *card = NULL;
    lv_obj_t *status_label = lv_label_create(lv_scr_act());
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(status_label, "Initializing SD Card...");
    lv_timer_handler();

    bool sd_status = SD_Init(&card);
    if (sd_status)
    {
        char card_info[64];
        snprintf(card_info, sizeof(card_info),
                 "Size: %lluMB\nName: %s",
                 ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024),
                 card->cid.name);
        lv_label_set_text(status_label, card_info);
    }
    else
    {
        lv_label_set_text(status_label, "SD Card Failed!");
    }

    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(2000));
    lv_obj_del(status_label);

    // while (1)
    // {
    //     vTaskDelay(pdMS_TO_TICKS(10));
    //     lv_timer_handler();
    // }
    return;
}
void record_wav(uint32_t rec_time)
{
    FILE *f = NULL;
    int flash_wr_size = 0;
    
    // Calculate recording parameters
    uint32_t flash_rec_time = BYTE_RATE * rec_time;
    const wav_header_t wav_header = WAV_HEADER_PCM_DEFAULT(
        flash_rec_time, 
        CONFIG_EXAMPLE_BIT_SAMPLE, 
        CONFIG_EXAMPLE_SAMPLE_RATE, 
        NUM_CHANNELS
    );

    // Check and delete existing file
    struct stat st;
    if (stat(SD_MOUNT_POINT"/record.wav", &st) == 0) {
        unlink(SD_MOUNT_POINT"/record.wav");
    }

    // Create new WAV file
    ESP_LOGI(TAG, "Opening file for writing");
    f = fopen(SD_MOUNT_POINT"/record.wav", "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file");
        return;
    }

    // Write WAV header
    size_t written = fwrite(&wav_header, sizeof(wav_header), 1, f);
    if (written != 1) {
        ESP_LOGE(TAG, "Header write failed");
        fclose(f);
        return;
    }

    // Recording loop
    ESP_LOGI(TAG, "Starting recording...");
    while (flash_wr_size < flash_rec_time) {
        if (i2s_channel_read(rx_handle, (char *)i2s_readraw_buff, SAMPLE_SIZE, &bytes_read, 1000) == ESP_OK) {
            size_t written = fwrite(i2s_readraw_buff, 1, bytes_read, f);
            if (written != bytes_read) {
                ESP_LOGE(TAG, "Write error: wrote %d/%d bytes", written, bytes_read);
                break;
            }
            flash_wr_size += written;
        } else {
            ESP_LOGE(TAG, "I2S read failed");
            break;
        }
    }
    fflush(f);
    // Cleanup
    fclose(f);
    ESP_LOGI(TAG, "Recording finished, wrote %d bytes", flash_wr_size);

    // Unmount SD card
    esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
    ESP_LOGI(TAG, "SD card unmounted");
    //spi_bus_free(host.slot);
}
void init_microphone(void){
    // I2S channel configuration
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    // PDM microphone configuration
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(CONFIG_EXAMPLE_SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = CONFIG_EXAMPLE_I2S_CLK_GPIO,
            .din = CONFIG_EXAMPLE_I2S_DATA_GPIO,
            .invert_flags = {.clk_inv = false},
        },
    };

    // Initialize and enable I2S
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}
void start_recording(void){
    printf("\nPDM Microphone Recording Example\n");
    printf("--------------------------------\n");

    // Initialize components
    //esp_log_level_set("vfs_fat_sdmmc", ESP_LOG_DEBUG);
    //mount_sdcard();
    //init_microphone(); //might need uncommenting

    // Start recording
    ESP_LOGI(TAG, "Starting %d second recording", CONFIG_EXAMPLE_REC_TIME);
    record_wav(CONFIG_EXAMPLE_REC_TIME);

    // Cleanup I2S
    ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));
    ESP_ERROR_CHECK(i2s_del_channel(rx_handle));
    
    printf("Recording completed successfully!\n");
}