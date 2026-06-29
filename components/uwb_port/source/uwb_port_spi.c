#include "uwb_port.h"

#include <stdlib.h>
#include <string.h>

#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "uwb_poc_config.h"

static const char *TAG = "UWB_PORT_SPI";

static spi_device_handle_t s_spi_device;
static SemaphoreHandle_t s_spi_mutex;
static bool s_spi_bus_initialized;
static int s_spi_clock_hz = UWB_POC_SPI_CLOCK_HZ;

static esp_err_t uwb_port_spi_add_device(void)
{
    spi_device_interface_config_t device_config = {
        .clock_speed_hz = s_spi_clock_hz,
        .mode = 0,
        .spics_io_num = UWB_POC_PIN_CS,
        .queue_size = 1,
    };

    esp_err_t ret = spi_bus_add_device(UWB_POC_SPI_HOST,
                                       &device_config,
                                       &s_spi_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPI device configured clock=%d Hz", s_spi_clock_hz);
    return ESP_OK;
}

static esp_err_t uwb_port_spi_lock(void)
{
    if (s_spi_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_spi_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void uwb_port_spi_unlock(void)
{
    if (s_spi_mutex != NULL) {
        (void)xSemaphoreGive(s_spi_mutex);
    }
}

esp_err_t uwb_port_spi_init(void)
{
    if (s_spi_device != NULL) {
        return ESP_OK;
    }

    if (s_spi_mutex == NULL) {
        s_spi_mutex = xSemaphoreCreateMutex();
        if (s_spi_mutex == NULL) {
            ESP_LOGE(TAG, "failed to create SPI mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (!s_spi_bus_initialized) {
        spi_bus_config_t bus_config = {
            .mosi_io_num = UWB_POC_PIN_MOSI,
            .miso_io_num = UWB_POC_PIN_MISO,
            .sclk_io_num = UWB_POC_PIN_SCLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 1024,
        };

        esp_err_t ret = spi_bus_initialize(UWB_POC_SPI_HOST,
                                           &bus_config,
                                           SPI_DMA_CH_AUTO);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
            return ret;
        }

        s_spi_bus_initialized = true;
    }

    esp_err_t ret = uwb_port_spi_add_device();
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "SPI initialized host=%d", UWB_POC_SPI_HOST);
    return ESP_OK;
}

int writetospi(uint16_t headerLength,
               const uint8_t *headerBuffer,
               uint16_t bodyLength,
               const uint8_t *bodyBuffer)
{
    if (headerLength == 0 ||
        headerBuffer == NULL ||
        (bodyLength > 0 && bodyBuffer == NULL)) {
        ESP_LOGE(TAG, "invalid SPI write request");
        return -1;
    }

    const size_t total_len = (size_t)headerLength + (size_t)bodyLength;
    uint8_t *tx_buffer = (uint8_t *)malloc(total_len);
    if (tx_buffer == NULL) {
        ESP_LOGE(TAG, "failed to allocate SPI write buffer");
        return -1;
    }

    memcpy(tx_buffer, headerBuffer, headerLength);
    if (bodyLength > 0) {
        memcpy(tx_buffer + headerLength, bodyBuffer, bodyLength);
    }

    spi_transaction_t transaction = {
        .length = total_len * 8,
        .tx_buffer = tx_buffer,
    };

    bool locked = false;
    esp_err_t ret = uwb_port_spi_lock();
    if (ret == ESP_OK) {
        locked = true;
    }

    if (ret == ESP_OK && s_spi_device == NULL) {
        ret = ESP_ERR_INVALID_STATE;
    }

    if (ret == ESP_OK) {
        ret = spi_device_polling_transmit(s_spi_device, &transaction);
    }
    if (locked) {
        uwb_port_spi_unlock();
    }

    free(tx_buffer);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI write failed: %s", esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int readfromspi(uint16_t headerLength,
                const uint8_t *headerBuffer,
                uint16_t readLength,
                uint8_t *readBuffer)
{
    if (headerLength == 0 ||
        headerBuffer == NULL ||
        readLength == 0 ||
        readBuffer == NULL) {
        ESP_LOGE(TAG, "invalid SPI read request");
        return -1;
    }

    const size_t total_len = (size_t)headerLength + (size_t)readLength;
    uint8_t *tx_buffer = (uint8_t *)calloc(total_len, sizeof(uint8_t));
    uint8_t *rx_buffer = (uint8_t *)calloc(total_len, sizeof(uint8_t));
    if (tx_buffer == NULL || rx_buffer == NULL) {
        ESP_LOGE(TAG, "failed to allocate SPI read buffers");
        free(tx_buffer);
        free(rx_buffer);
        return -1;
    }

    memcpy(tx_buffer, headerBuffer, headerLength);

    spi_transaction_t transaction = {
        .length = total_len * 8,
        .tx_buffer = tx_buffer,
        .rx_buffer = rx_buffer,
    };

    bool locked = false;
    esp_err_t ret = uwb_port_spi_lock();
    if (ret == ESP_OK) {
        locked = true;
    }

    if (ret == ESP_OK && s_spi_device == NULL) {
        ret = ESP_ERR_INVALID_STATE;
    }

    if (ret == ESP_OK) {
        ret = spi_device_polling_transmit(s_spi_device, &transaction);
    }
    if (locked) {
        uwb_port_spi_unlock();
    }

    if (ret == ESP_OK) {
        memcpy(readBuffer, rx_buffer + headerLength, readLength);
    }

    free(tx_buffer);
    free(rx_buffer);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI read failed: %s", esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int writetospiwithcrc(uint16_t headerLength,
                      const uint8_t *headerBuffer,
                      uint16_t bodyLength,
                      const uint8_t *bodyBuffer,
                      uint8_t crc8)
{
    uint8_t *crc_body = (uint8_t *)malloc((size_t)bodyLength + 1U);
    if (crc_body == NULL) {
        ESP_LOGE(TAG, "failed to allocate SPI CRC write buffer");
        return -1;
    }

    if (bodyLength > 0 && bodyBuffer != NULL) {
        memcpy(crc_body, bodyBuffer, bodyLength);
    }
    crc_body[bodyLength] = crc8;

    int ret = writetospi(headerLength, headerBuffer, bodyLength + 1U, crc_body);
    free(crc_body);
    return ret;
}

static void uwb_port_spi_set_clock(int clock_hz)
{
    if (s_spi_mutex == NULL) {
        s_spi_clock_hz = clock_hz;
        return;
    }

    if (uwb_port_spi_lock() != ESP_OK) {
        ESP_LOGE(TAG, "failed to lock SPI for clock change");
        return;
    }

    if (s_spi_device != NULL) {
        esp_err_t ret = spi_bus_remove_device(s_spi_device);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "spi_bus_remove_device failed: %s", esp_err_to_name(ret));
            uwb_port_spi_unlock();
            return;
        }
        s_spi_device = NULL;
    }

    s_spi_clock_hz = clock_hz;

    if (s_spi_bus_initialized) {
        esp_err_t ret = uwb_port_spi_add_device();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI clock reconfigure failed: %s", esp_err_to_name(ret));
        }
    }

    uwb_port_spi_unlock();
}

void port_set_dw_ic_spi_slowrate(void)
{
    uwb_port_spi_set_clock(2 * 1000 * 1000);
    ESP_LOGI(TAG, "DW3000 SPI slow rate requested");
}

void port_set_dw_ic_spi_fastrate(void)
{
    uwb_port_spi_set_clock(UWB_POC_SPI_CLOCK_HZ);
    ESP_LOGI(TAG, "DW3000 SPI fast rate requested");
}
