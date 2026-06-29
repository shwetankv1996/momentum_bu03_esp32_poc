#include "uwb_port.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "uwb_poc_config.h"

static const char *TAG = "UWB_PORT_IRQ";

esp_err_t uwb_port_irq_init(void)
{
    gpio_config_t irq_config = {
        .pin_bit_mask = 1ULL << UWB_POC_PIN_IRQ,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&irq_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IRQ GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "IRQ initialized GPIO=%d", UWB_POC_PIN_IRQ);
    return ESP_OK;
}

bool uwb_port_irq_is_asserted(void)
{
    return gpio_get_level(UWB_POC_PIN_IRQ) != 0;
}
