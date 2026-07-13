#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "uwb_poc.h"
#include "uwb_poc_config.h"

static const char *TAG = "MOMENTUM_UWB_APP";

static const char *uwb_role_to_string(uwb_role_t role)
{
    switch (role) {
    case UWB_ROLE_TAG:
        return "TAG";
    case UWB_ROLE_ANCHOR:
        return "ANCHOR";
    default:
        return "UNKNOWN";
    }
}

static const char *uwb_mode_to_string(uwb_poc_mode_t mode)
{
    switch (mode) {
    case UWB_POC_MODE_SIMPLE_TX:
        return "SIMPLE_TX";
    case UWB_POC_MODE_SIMPLE_RX:
        return "SIMPLE_RX";
    case UWB_POC_MODE_DS_TWR:
        return "DS_TWR";
    default:
        return "UNKNOWN";
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Momentum BU03 ESP32 POC boot");
    ESP_LOGI(TAG,
             "UWB config: role=%s mode=%s node_id=%u channel=%u data_rate=%u sts=%u",
             uwb_role_to_string(UWB_POC_DEFAULT_ROLE),
             uwb_mode_to_string(UWB_POC_DEFAULT_MODE),
             UWB_POC_DEFAULT_NODE_ID,
             UWB_POC_DEFAULT_CHANNEL,
             UWB_POC_DEFAULT_DATA_RATE,
             UWB_POC_ENABLE_STS);

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
                         "Distance between TAG and ANCHOR: anchor=%u distance=%.2f m timestamp=%lu",
                         range.anchor_id,
                         range.distance_m,
                         (unsigned long)range.timestamp_ms);
            } else {
                ESP_LOGW(TAG, "Range: not valid yet");
            }
        }

        if (uwb_poc_get_diag(&diag) == ESP_OK) {
            ESP_LOGI(TAG,
                     "Diag: dev=0x%08lx state=%d role=%s mode=%s tx=%lu rx_ok=%lu "
                     "rx_to=%lu rx_err=%lu simple_tx=%lu simple_rx_ok=%lu "
                     "simple_rx_to=%lu simple_rx_err=%lu last_status=0x%08lx "
                     "range_ok=%lu range_bad=%lu last_range=%.2f m",
                     (unsigned long)diag.device_id,
                     diag.state,
                     uwb_role_to_string(diag.role),
                     uwb_mode_to_string(diag.mode),
                     (unsigned long)diag.tx_count,
                     (unsigned long)diag.rx_ok_count,
                     (unsigned long)diag.rx_timeout_count,
                     (unsigned long)diag.rx_error_count,
                     (unsigned long)diag.simple_tx_count,
                     (unsigned long)diag.simple_rx_ok_count,
                     (unsigned long)diag.simple_rx_timeout_count,
                     (unsigned long)diag.simple_rx_error_count,
                     (unsigned long)diag.last_rx_status,
                     (unsigned long)diag.range_ok_count,
                     (unsigned long)diag.range_invalid_count,
                     diag.last_range_m);
        }

        vTaskDelay(pdMS_TO_TICKS(UWB_POC_DIAG_PRINT_PERIOD_MS));
    }
}
