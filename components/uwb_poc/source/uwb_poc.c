#include "uwb_poc.h"

#include "deca_device_api.h"
#include "esp_log.h"
#include "uwb_poc_config.h"
#include "uwb_poc_internal.h"
#include "uwb_port.h"

static const char *TAG = "UWB_POC";

static uwb_poc_state_t s_state = UWB_POC_STATE_IDLE;
static uwb_role_t s_role = UWB_POC_DEFAULT_ROLE;
static bool s_initialized;
static bool s_running;

static bool uwb_poc_role_is_valid(uwb_role_t role)
{
    return role == UWB_ROLE_TAG || role == UWB_ROLE_ANCHOR;
}

static void uwb_poc_set_state(uwb_poc_state_t state)
{
    s_state = state;
    uwb_poc_diag_set_state(state);
}

esp_err_t uwb_poc_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    uwb_poc_diag_reset();
    uwb_poc_diag_set_role(s_role);
    uwb_poc_diag_set_state(UWB_POC_STATE_IDLE);
    uwb_poc_diag_inc_init();

    esp_err_t ret = uwb_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "port init failed: %s", esp_err_to_name(ret));
        uwb_poc_set_state(UWB_POC_STATE_ERROR);
        return ret;
    }

    uwb_poc_diag_set_spi_ok(true);
    uwb_poc_diag_set_gpio_ok(true);
    uwb_poc_diag_set_irq_ok(true);

    uwb_port_reset();
    uwb_poc_diag_inc_reset();

    uint32_t device_id = 0;
    ret = uwb_poc_read_device_id(&device_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "device ID read failed: %s", esp_err_to_name(ret));
        uwb_poc_set_state(UWB_POC_STATE_ERROR);
        return ret;
    }

    uwb_poc_diag_set_device_id(device_id);
    ESP_LOGI(TAG, "device id = 0x%08lx", (unsigned long)device_id);

    if (device_id == 0 || device_id == 0xFFFFFFFFU) {
        ESP_LOGE(TAG, "invalid DW3000 device ID: 0x%08lx", (unsigned long)device_id);
        uwb_poc_set_state(UWB_POC_STATE_ERROR);
        return ESP_ERR_NOT_FOUND;
    }

    ret = uwb_poc_driver_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DW3000 init failed: %s", esp_err_to_name(ret));
        uwb_poc_set_state(UWB_POC_STATE_ERROR);
        return ret;
    }

    ret = uwb_poc_apply_default_config();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PHY config failed: %s", esp_err_to_name(ret));
        uwb_poc_set_state(UWB_POC_STATE_ERROR);
        return ret;
    }

    s_initialized = true;
    uwb_poc_set_state(UWB_POC_STATE_INITIALIZED);
    ESP_LOGI(TAG, "UWB POC initialized");
    return ESP_OK;
}

esp_err_t uwb_poc_set_role(uwb_role_t role)
{
    if (!uwb_poc_role_is_valid(role)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_running) {
        return ESP_ERR_INVALID_STATE;
    }

    s_role = role;
    uwb_poc_diag_set_role(role);
    ESP_LOGI(TAG, "selected role=%d", role);
    return ESP_OK;
}

esp_err_t uwb_poc_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_running) {
        return ESP_OK;
    }

    esp_err_t ret = uwb_poc_ranging_start(s_role);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ranging start failed: %s", esp_err_to_name(ret));
        uwb_poc_set_state(UWB_POC_STATE_ERROR);
        return ret;
    }

    s_running = true;
    uwb_poc_set_state(UWB_POC_STATE_RUNNING);
    ESP_LOGI(TAG, "ranging started");
    return ESP_OK;
}

esp_err_t uwb_poc_stop(void)
{
    if (!s_running) {
        return ESP_OK;
    }

    esp_err_t ret = uwb_poc_ranging_stop();
    dwt_forcetrxoff();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ranging stop failed: %s", esp_err_to_name(ret));
        uwb_poc_set_state(UWB_POC_STATE_ERROR);
        return ret;
    }

    s_running = false;
    uwb_poc_set_state(UWB_POC_STATE_INITIALIZED);
    ESP_LOGI(TAG, "ranging stopped");
    return ESP_OK;
}
