#include "uwb_port.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t uwb_port_init(void)
{
    esp_err_t ret = uwb_port_spi_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = uwb_port_gpio_init();
    if (ret != ESP_OK) {
        return ret;
    }

    return uwb_port_irq_init();
}

void uwb_port_sleep_ms(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

uint32_t uwb_port_get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

void deca_sleep(unsigned int time_ms)
{
    uwb_port_sleep_ms((uint32_t)time_ms);
}
