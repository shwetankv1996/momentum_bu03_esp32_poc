#include "uwb_port.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "uwb_poc_config.h"

static const char *TAG = "UWB_PORT_GPIO";

static esp_err_t uwb_port_reset_drive_low(void)
{
    gpio_config_t reset_config = {
        .pin_bit_mask = 1ULL << UWB_POC_PIN_RST,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&reset_config);
    if (ret != ESP_OK) {
        return ret;
    }

    return gpio_set_level(UWB_POC_PIN_RST, 0);
}

static esp_err_t uwb_port_reset_release(void)
{
    gpio_config_t reset_config = {
        .pin_bit_mask = 1ULL << UWB_POC_PIN_RST,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&reset_config);
}

esp_err_t uwb_port_gpio_init(void)
{
    esp_err_t ret = uwb_port_reset_release();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RST GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "GPIO initialized RST=%d", UWB_POC_PIN_RST);
    return ESP_OK;
}

void uwb_port_reset(void)
{
    ESP_LOGI(TAG, "resetting BU03/DW3000");
    esp_err_t ret = uwb_port_reset_drive_low();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to drive reset low: %s", esp_err_to_name(ret));
        return;
    }

    uwb_port_sleep_ms(2);

    ret = uwb_port_reset_release();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to release reset: %s", esp_err_to_name(ret));
        return;
    }

    uwb_port_sleep_ms(10);
}
