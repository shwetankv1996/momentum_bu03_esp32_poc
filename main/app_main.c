#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "uwb_poc.h"
#include "uwb_poc_config.h"

static const char *TAG = "MOMENTUM_UWB_APP";

void app_main(void)
{
    ESP_LOGI(TAG, "Momentum BU03 ESP32 POC boot");

    esp_err_t ret = uwb_poc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uwb_poc_init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = uwb_poc_set_role(UWB_POC_DEFAULT_ROLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uwb_poc_set_role failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = uwb_poc_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uwb_poc_start failed: %s", esp_err_to_name(ret));
        return;
    }

    while (1) {
        uwb_range_t range = {0};
        uwb_diag_t diag = {0};

        if (uwb_poc_get_latest_range(&range) == ESP_OK) {
            if (range.valid) {
                ESP_LOGI(TAG,
                         "Range: anchor=%u distance=%.2f m timestamp=%lu",
                         range.anchor_id,
                         range.distance_m,
                         (unsigned long)range.timestamp_ms);
            } else {
                ESP_LOGW(TAG, "Range: not valid yet");
            }
        }

        if (uwb_poc_get_diag(&diag) == ESP_OK) {
            ESP_LOGI(TAG,
                     "Diag: dev=0x%08lx state=%d role=%d tx=%lu rx_ok=%lu "
                     "rx_to=%lu rx_err=%lu range_ok=%lu range_bad=%lu",
                     (unsigned long)diag.device_id,
                     diag.state,
                     diag.role,
                     (unsigned long)diag.tx_count,
                     (unsigned long)diag.rx_ok_count,
                     (unsigned long)diag.rx_timeout_count,
                     (unsigned long)diag.rx_error_count,
                     (unsigned long)diag.range_ok_count,
                     (unsigned long)diag.range_invalid_count);
        }

        vTaskDelay(pdMS_TO_TICKS(UWB_POC_DIAG_PRINT_PERIOD_MS));
    }
}
