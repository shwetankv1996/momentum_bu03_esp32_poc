#include "uwb_poc_internal.h"

#include "deca_device_api.h"
#include "deca_probe_interface.h"
#include "esp_log.h"
#include "uwb_poc_config.h"
#include "uwb_port.h"

static const char *TAG = "UWB_POC_CONFIG";

static bool s_driver_probed;

static esp_err_t uwb_poc_read_raw_device_id(uint32_t *device_id)
{
    if (device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t header = 0x00U;
    uint8_t buffer[sizeof(uint32_t)] = { 0 };
    int ret = readfromspi(sizeof(header), &header, sizeof(buffer), buffer);
    if (ret != 0) {
        ESP_LOGE(TAG, "raw DW3000 device ID read failed");
        return ESP_FAIL;
    }

    *device_id = ((uint32_t)buffer[3] << 24U) |
                 ((uint32_t)buffer[2] << 16U) |
                 ((uint32_t)buffer[1] << 8U) |
                 (uint32_t)buffer[0];
    return ESP_OK;
}

static esp_err_t uwb_poc_probe_driver(void)
{
    if (s_driver_probed) {
        return ESP_OK;
    }

    port_set_dw_ic_spi_slowrate();

    int32_t ret = dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf);
    if (ret != DWT_SUCCESS) {
        ESP_LOGE(TAG, "dwt_probe failed: %ld", (long)ret);
        return ESP_ERR_NOT_FOUND;
    }

    s_driver_probed = true;
    return ESP_OK;
}

esp_err_t uwb_poc_read_device_id(uint32_t *device_id)
{
    if (device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *device_id = 0;

    esp_err_t ret = uwb_poc_read_raw_device_id(device_id);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "DW3000 device ID read: 0x%08lx", (unsigned long)*device_id);
    return ESP_OK;
}

esp_err_t uwb_poc_driver_init(void)
{
    esp_err_t ret = uwb_poc_probe_driver();
    if (ret != ESP_OK) {
        return ret;
    }

    port_set_dw_ic_spi_slowrate();

    int32_t init_ret = dwt_initialise(DWT_READ_OTP_PID |
                                      DWT_READ_OTP_LID |
                                      DWT_READ_OTP_BAT |
                                      DWT_READ_OTP_TMP);
    if (init_ret != DWT_SUCCESS) {
        ESP_LOGE(TAG, "dwt_initialise failed: %ld", (long)init_ret);
        return ESP_FAIL;
    }

    (void)dwt_setdwstate(DWT_DW_IDLE);

    ESP_LOGI(TAG, "DW3000 driver initialized");
    return ESP_OK;
}

esp_err_t uwb_poc_apply_default_config(void)
{
    dwt_config_t config = {
        .chan = UWB_POC_DEFAULT_CHANNEL,
        .txPreambLength = DWT_PLEN_64,
        .rxPAC = DWT_PAC8,
        .txCode = 9,
        .rxCode = 9,
        .sfdType = DWT_SFD_IEEE_4Z,
        .dataRate = DWT_BR_6M8,
        .phrMode = DWT_PHRMODE_STD,
        .phrRate = DWT_PHRRATE_STD,
        .sfdTO = 129,
#if UWB_POC_ENABLE_STS
        .stsMode = (dwt_sts_mode_e)(DWT_STS_MODE_1 | DWT_STS_MODE_SDC),
#else
        .stsMode = DWT_STS_MODE_OFF,
#endif
        .stsLength = DWT_STS_LEN_64,
        .pdoaMode = DWT_PDOA_M0,
    };

    int32_t ret = dwt_configure(&config);
    if (ret != DWT_SUCCESS) {
        ESP_LOGE(TAG, "dwt_configure failed: %ld", (long)ret);
        return ESP_FAIL;
    }

    dwt_setrxantennadelay(0);
    dwt_settxantennadelay(0);
    dwt_configuretxrf(&(dwt_txconfig_t) {
        .PGdly = 0x34,
        .power = 0xFFFFFFFFUL,
        .PGcount = 0,
    });

    port_set_dw_ic_spi_fastrate();

    ESP_LOGI(TAG,
             "DW3000 PHY configured ch=%u rate=%u kbps plen=64 pac=8 code=9 sts=%u",
             UWB_POC_DEFAULT_CHANNEL,
             UWB_POC_DEFAULT_DATA_RATE,
             UWB_POC_ENABLE_STS);
    return ESP_OK;
}
