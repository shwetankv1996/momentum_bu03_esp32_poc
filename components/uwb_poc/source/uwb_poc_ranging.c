#include "uwb_poc.h"

#include <stdbool.h>
#include <string.h>

#include "deca_device_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "uwb_poc_config.h"
#include "uwb_poc_internal.h"
#include "uwb_port.h"

static const char *TAG = "UWB_RANGE";

static SemaphoreHandle_t s_range_mutex;
static uwb_range_t s_latest_range;

static TaskHandle_t s_ranging_task_handle;
static volatile bool s_ranging_stop_requested;
static uwb_role_t s_ranging_role = UWB_ROLE_TAG;
static uwb_poc_mode_t s_ranging_mode = UWB_POC_DEFAULT_MODE;

#define UWB_FRAME_MAGIC0 0x4DU
#define UWB_FRAME_MAGIC1 0x55U
#define UWB_FRAME_TYPE_POLL 0x01U
#define UWB_FRAME_TYPE_RESP 0x02U
#define UWB_FRAME_TYPE_FINAL 0x03U
#define UWB_FRAME_TYPE_SIMPLE 0x55U
#define UWB_FRAME_HEADER_LEN 8U
#define UWB_FRAME_FINAL_LEN 20U
#define UWB_SIMPLE_FRAME_LEN 12U
#define UWB_FRAME_IDX_MAGIC0 0U
#define UWB_FRAME_IDX_MAGIC1 1U
#define UWB_FRAME_IDX_TYPE 2U
#define UWB_FRAME_IDX_SEQ 3U
#define UWB_FRAME_IDX_TAG_ID 4U
#define UWB_FRAME_IDX_ANCHOR_ID 6U
#define UWB_FRAME_IDX_POLL_TX_TS 8U
#define UWB_FRAME_IDX_RESP_RX_TS 12U
#define UWB_FRAME_IDX_FINAL_TX_TS 16U

#define UWB_RX_TIMEOUT_UUS 10000U
#define UWB_RX_AFTER_TX_DELAY_UUS 150U
#define UWB_TAG_FINAL_DELAY_US 20000U
#define UWB_RANGE_WAIT_MAX_MS 50U
#define UWB_SPEED_OF_LIGHT_MPS 299702547.0

static bool uwb_poc_range_ensure_mutex(void)
{
    if (s_range_mutex == NULL) {
        s_range_mutex = xSemaphoreCreateMutex();
    }

    return s_range_mutex != NULL;
}

static void uwb_poc_range_lock(void)
{
    if (uwb_poc_range_ensure_mutex()) {
        (void)xSemaphoreTake(s_range_mutex, portMAX_DELAY);
    }
}

static void uwb_poc_range_unlock(void)
{
    if (s_range_mutex != NULL) {
        (void)xSemaphoreGive(s_range_mutex);
    }
}

static void uwb_frame_put_u16(uint8_t *frame, uint16_t offset, uint16_t value)
{
    frame[offset] = (uint8_t)(value & 0xFFU);
    frame[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
}

static uint16_t uwb_frame_get_u16(const uint8_t *frame, uint16_t offset)
{
    return (uint16_t)frame[offset] | ((uint16_t)frame[offset + 1U] << 8U);
}

static void uwb_frame_put_u32(uint8_t *frame, uint16_t offset, uint32_t value)
{
    frame[offset] = (uint8_t)(value & 0xFFU);
    frame[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
    frame[offset + 2U] = (uint8_t)((value >> 16U) & 0xFFU);
    frame[offset + 3U] = (uint8_t)((value >> 24U) & 0xFFU);
}

static uint32_t uwb_frame_get_u32(const uint8_t *frame, uint16_t offset)
{
    return (uint32_t)frame[offset] |
           ((uint32_t)frame[offset + 1U] << 8U) |
           ((uint32_t)frame[offset + 2U] << 16U) |
           ((uint32_t)frame[offset + 3U] << 24U);
}

static void uwb_frame_init(uint8_t *frame,
                           uint8_t type,
                           uint8_t seq,
                           uint16_t tag_id,
                           uint16_t anchor_id)
{
    frame[UWB_FRAME_IDX_MAGIC0] = UWB_FRAME_MAGIC0;
    frame[UWB_FRAME_IDX_MAGIC1] = UWB_FRAME_MAGIC1;
    frame[UWB_FRAME_IDX_TYPE] = type;
    frame[UWB_FRAME_IDX_SEQ] = seq;
    uwb_frame_put_u16(frame, UWB_FRAME_IDX_TAG_ID, tag_id);
    uwb_frame_put_u16(frame, UWB_FRAME_IDX_ANCHOR_ID, anchor_id);
}

static bool uwb_frame_matches(const uint8_t *frame, uint8_t type, uint8_t seq)
{
    return frame[UWB_FRAME_IDX_MAGIC0] == UWB_FRAME_MAGIC0 &&
           frame[UWB_FRAME_IDX_MAGIC1] == UWB_FRAME_MAGIC1 &&
           frame[UWB_FRAME_IDX_TYPE] == type &&
           frame[UWB_FRAME_IDX_SEQ] == seq;
}

static uint64_t uwb_get_rx_timestamp_u64(void)
{
    uint8_t ts[5] = {0};
    uint64_t value = 0;

    dwt_readrxtimestamp(ts, DWT_COMPAT_NONE);
    for (int i = 4; i >= 0; --i) {
        value <<= 8;
        value |= ts[i];
    }

    return value;
}

static void uwb_poc_ranging_set_latest(float distance_m, bool valid)
{
    uwb_poc_range_lock();
    s_latest_range.anchor_id = UWB_POC_ANCHOR_NODE_ID;
    s_latest_range.distance_m = distance_m;
    s_latest_range.timestamp_ms = uwb_port_get_time_ms();
    s_latest_range.valid = valid;
    uwb_poc_range_unlock();
    uwb_poc_diag_update_range(distance_m, valid);
}

static void uwb_poc_ranging_set_invalid(void)
{
    uwb_poc_ranging_set_latest(0.0f, false);
}

static void uwb_clear_events(void)
{
    dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK |
                         SYS_STATUS_ALL_RX_GOOD |
                         SYS_STATUS_ALL_RX_TO |
                         SYS_STATUS_ALL_RX_ERR);
}

static void uwb_clear_rx_events(void)
{
    dwt_writesysstatuslo(SYS_STATUS_ALL_RX_GOOD |
                         SYS_STATUS_ALL_RX_TO |
                         SYS_STATUS_ALL_RX_ERR);
}

static void uwb_log_rx_status(const char *ctx, uint32_t status)
{
    uwb_poc_diag_set_last_rx_status(status);
    ESP_LOGW(TAG,
             "%s status=0x%08lx rxfcg=%d rx_to=0x%08lx rx_err=0x%08lx irq=%d",
             ctx,
             (unsigned long)status,
             (status & DWT_INT_RXFCG_BIT_MASK) ? 1 : 0,
             (unsigned long)(status & SYS_STATUS_ALL_RX_TO),
             (unsigned long)(status & SYS_STATUS_ALL_RX_ERR),
             uwb_port_irq_is_asserted() ? 1 : 0);
}

static bool uwb_wait_for_status(uint32_t mask, uint32_t *status_out)
{
    const uint32_t started_ms = uwb_port_get_time_ms();

    while (!s_ranging_stop_requested) {
        uint32_t status = dwt_readsysstatuslo();
        if ((status & mask) != 0U) {
            if (status_out != NULL) {
                *status_out = status;
            }
            return true;
        }

        if ((uwb_port_get_time_ms() - started_ms) > UWB_RANGE_WAIT_MAX_MS) {
            if (status_out != NULL) {
                *status_out = status;
            }
            return false;
        }

        vTaskDelay(1);
    }

    return false;
}

static bool uwb_send_frame(uint8_t *frame,
                           uint16_t payload_len,
                           uint8_t tx_mode,
                           bool wait_tx_done)
{
    if (dwt_writetxdata(payload_len, frame, 0) != DWT_SUCCESS) {
        ESP_LOGE(TAG, "dwt_writetxdata failed");
        return false;
    }

    dwt_writetxfctrl(payload_len + FCS_LEN, 0, 1);

    if (dwt_starttx(tx_mode) != DWT_SUCCESS) {
        ESP_LOGE(TAG, "dwt_starttx failed mode=0x%02x", tx_mode);
        return false;
    }

    uwb_poc_diag_inc_tx();

    if (!wait_tx_done) {
        return true;
    }

    uint32_t status = 0;
    if (!uwb_wait_for_status(DWT_INT_TXFRS_BIT_MASK, &status)) {
        ESP_LOGW(TAG, "TX done wait timeout status=0x%08lx", (unsigned long)status);
        return false;
    }

    dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
    return true;
}

static bool uwb_wait_for_rx(uint32_t *status_out, bool mark_range_invalid, const char *ctx)
{
    const uint32_t rx_mask = DWT_INT_RXFCG_BIT_MASK |
                             SYS_STATUS_ALL_RX_TO |
                             SYS_STATUS_ALL_RX_ERR;

    uint32_t status = 0;
    bool got_status = uwb_wait_for_status(rx_mask, &status);
    if (status_out != NULL) {
        *status_out = status;
    }

    uwb_poc_diag_set_last_rx_status(status);

    if (!got_status) {
        uwb_poc_diag_inc_rx_timeout();
        uwb_log_rx_status(ctx, status);
        if (mark_range_invalid) {
            uwb_poc_ranging_set_invalid();
        }
        return false;
    }

    if ((status & DWT_INT_RXFCG_BIT_MASK) != 0U) {
        uwb_poc_diag_inc_rx_ok();
        return true;
    }

    if ((status & SYS_STATUS_ALL_RX_TO) != 0U) {
        uwb_poc_diag_inc_rx_timeout();
    } else {
        uwb_poc_diag_inc_rx_error();
    }

    uwb_log_rx_status(ctx, status);
    dwt_writesysstatuslo(status & (SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR));
    if (mark_range_invalid) {
        uwb_poc_ranging_set_invalid();
    }
    return false;
}

static void uwb_simple_tx_step(uint8_t *seq)
{
    uint8_t frame[UWB_SIMPLE_FRAME_LEN] = {0};

    uwb_frame_init(frame,
                   UWB_FRAME_TYPE_SIMPLE,
                   *seq,
                   UWB_POC_TAG_NODE_ID,
                   UWB_POC_ANCHOR_NODE_ID);
    frame[8] = 0xA1U;
    frame[9] = 0xB2U;
    frame[10] = 0xC3U;
    frame[11] = 0xD4U;

    uwb_clear_events();

    if (uwb_send_frame(frame, sizeof(frame), DWT_START_TX_IMMEDIATE, true)) {
        uwb_poc_diag_inc_simple_tx();
        ESP_LOGI(TAG, "SIMPLE_TX sent seq=%u", *seq);
        (*seq)++;
    } else {
        ESP_LOGW(TAG, "SIMPLE_TX failed seq=%u", *seq);
    }

    vTaskDelay(pdMS_TO_TICKS(UWB_POC_SIMPLE_TX_PERIOD_MS));
}

static void uwb_simple_rx_step(void)
{
    uint8_t frame[32] = {0};

    uwb_clear_rx_events();
    dwt_setrxtimeout(UWB_RX_TIMEOUT_UUS);

    if (dwt_rxenable(DWT_START_RX_IMMEDIATE) != DWT_SUCCESS) {
        ESP_LOGE(TAG, "SIMPLE_RX dwt_rxenable failed");
        uwb_poc_diag_inc_rx_error();
        uwb_poc_diag_inc_simple_rx_error();
        vTaskDelay(pdMS_TO_TICKS(UWB_POC_DEBUG_STEP_DELAY_MS));
        return;
    }

    uint32_t status = 0;
    if (!uwb_wait_for_rx(&status, false, "SIMPLE_RX")) {
        if ((status & SYS_STATUS_ALL_RX_TO) != 0U || status == 0U) {
            uwb_poc_diag_inc_simple_rx_timeout();
        } else {
            uwb_poc_diag_inc_simple_rx_error();
        }
        uwb_clear_rx_events();
        vTaskDelay(pdMS_TO_TICKS(UWB_POC_DEBUG_STEP_DELAY_MS));
        return;
    }

    dwt_readrxdata(frame, sizeof(frame), 0);
    dwt_writesysstatuslo(status & SYS_STATUS_ALL_RX_GOOD);
    uwb_poc_diag_inc_simple_rx_ok();

    ESP_LOGI(TAG,
             "SIMPLE_RX ok raw=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
             frame[0], frame[1], frame[2], frame[3],
             frame[4], frame[5], frame[6], frame[7],
             frame[8], frame[9], frame[10], frame[11]);

    if (!uwb_frame_matches(frame, UWB_FRAME_TYPE_SIMPLE, frame[UWB_FRAME_IDX_SEQ])) {
        ESP_LOGW(TAG, "SIMPLE_RX received non-debug frame type=0x%02x seq=%u",
                 frame[UWB_FRAME_IDX_TYPE],
                 frame[UWB_FRAME_IDX_SEQ]);
    }
}

static void uwb_tag_step(uint8_t *seq)
{
    uint8_t poll[UWB_FRAME_HEADER_LEN] = {0};
    uint8_t resp[UWB_FRAME_HEADER_LEN] = {0};
    uint8_t final[UWB_FRAME_FINAL_LEN] = {0};

    uwb_frame_init(poll,
                   UWB_FRAME_TYPE_POLL,
                   *seq,
                   UWB_POC_TAG_NODE_ID,
                   UWB_POC_ANCHOR_NODE_ID);

    uwb_clear_events();
    dwt_setrxaftertxdelay(UWB_RX_AFTER_TX_DELAY_UUS);
    dwt_setrxtimeout(UWB_RX_TIMEOUT_UUS);

    if (!uwb_send_frame(poll,
                        sizeof(poll),
                        DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED,
                        false)) {
        uwb_poc_ranging_set_invalid();
        return;
    }

    uint32_t status = 0;
    if (!uwb_wait_for_rx(&status, true, "TAG_WAIT_RESP")) {
        return;
    }

    const uint32_t poll_tx_ts = dwt_readtxtimestamplo32();
    const uint64_t resp_rx_ts_40 = uwb_get_rx_timestamp_u64();
    const uint32_t resp_rx_ts = (uint32_t)resp_rx_ts_40;

    dwt_readrxdata(resp, sizeof(resp), 0);
    dwt_writesysstatuslo(status & SYS_STATUS_ALL_RX_GOOD);

    if (!uwb_frame_matches(resp, UWB_FRAME_TYPE_RESP, *seq)) {
        ESP_LOGW(TAG, "unexpected RESP frame type=0x%02x seq=%u expected=%u",
                 resp[UWB_FRAME_IDX_TYPE],
                 resp[UWB_FRAME_IDX_SEQ],
                 *seq);
        uwb_poc_diag_inc_rx_error();
        uwb_poc_ranging_set_invalid();
        return;
    }

    const uint16_t anchor_id = uwb_frame_get_u16(resp, UWB_FRAME_IDX_ANCHOR_ID);
    const uint32_t final_tx_time = (uint32_t)((resp_rx_ts_40 + US_TO_DTU(UWB_TAG_FINAL_DELAY_US)) >> 8);
    const uint32_t final_tx_ts = (uint32_t)(((uint64_t)(final_tx_time & 0xFFFFFFFEUL)) << 8);

    ESP_LOGI(TAG,
             "schedule FINAL seq=%u anchor=%u tx_time=0x%08lx delay=%u us",
             *seq,
             anchor_id,
             (unsigned long)final_tx_time,
             UWB_TAG_FINAL_DELAY_US);

    uwb_frame_init(final,
                   UWB_FRAME_TYPE_FINAL,
                   *seq,
                   UWB_POC_TAG_NODE_ID,
                   anchor_id);
    uwb_frame_put_u32(final, UWB_FRAME_IDX_POLL_TX_TS, poll_tx_ts);
    uwb_frame_put_u32(final, UWB_FRAME_IDX_RESP_RX_TS, resp_rx_ts);
    uwb_frame_put_u32(final, UWB_FRAME_IDX_FINAL_TX_TS, final_tx_ts);

    uwb_clear_events();
    dwt_setdelayedtrxtime(final_tx_time);

    if (!uwb_send_frame(final,
                        sizeof(final),
                        DWT_START_TX_DELAYED,
                        true)) {
        uwb_poc_ranging_set_invalid();
        return;
    }

    (*seq)++;
    vTaskDelay(pdMS_TO_TICKS(UWB_POC_DEBUG_STEP_DELAY_MS));
}

static void uwb_anchor_step(void)
{
    uint8_t poll[UWB_FRAME_HEADER_LEN] = {0};
    uint8_t resp[UWB_FRAME_HEADER_LEN] = {0};
    uint8_t final[UWB_FRAME_FINAL_LEN] = {0};

    uwb_clear_rx_events();
    dwt_setrxtimeout(UWB_RX_TIMEOUT_UUS);

    if (dwt_rxenable(DWT_START_RX_IMMEDIATE) != DWT_SUCCESS) {
        ESP_LOGE(TAG, "dwt_rxenable failed");
        uwb_poc_diag_inc_rx_error();
        uwb_poc_ranging_set_invalid();
        vTaskDelay(pdMS_TO_TICKS(UWB_POC_DEBUG_STEP_DELAY_MS));
        return;
    }

    uint32_t status = 0;
    if (!uwb_wait_for_rx(&status, true, "ANCHOR_WAIT_POLL")) {
        return;
    }

    const uint32_t poll_rx_ts = dwt_readrxtimestamplo32(DWT_COMPAT_NONE);

    dwt_readrxdata(poll, sizeof(poll), 0);
    dwt_writesysstatuslo(status & SYS_STATUS_ALL_RX_GOOD);

    if (!uwb_frame_matches(poll, UWB_FRAME_TYPE_POLL, poll[UWB_FRAME_IDX_SEQ])) {
        ESP_LOGW(TAG, "unexpected POLL frame type=0x%02x seq=%u",
                 poll[UWB_FRAME_IDX_TYPE],
                 poll[UWB_FRAME_IDX_SEQ]);
        uwb_poc_diag_inc_rx_error();
        uwb_poc_ranging_set_invalid();
        return;
    }

    const uint8_t seq = poll[UWB_FRAME_IDX_SEQ];
    const uint16_t tag_id = uwb_frame_get_u16(poll, UWB_FRAME_IDX_TAG_ID);

    uwb_frame_init(resp,
                   UWB_FRAME_TYPE_RESP,
                   seq,
                   tag_id,
                   UWB_POC_ANCHOR_NODE_ID);

    uwb_clear_events();
    dwt_setrxaftertxdelay(UWB_RX_AFTER_TX_DELAY_UUS);
    dwt_setrxtimeout(UWB_RX_TIMEOUT_UUS);

    if (!uwb_send_frame(resp,
                        sizeof(resp),
                        DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED,
                        false)) {
        uwb_poc_ranging_set_invalid();
        return;
    }

    if (!uwb_wait_for_rx(&status, true, "ANCHOR_WAIT_FINAL")) {
        return;
    }

    const uint32_t resp_tx_ts = dwt_readtxtimestamplo32();
    const uint32_t final_rx_ts = dwt_readrxtimestamplo32(DWT_COMPAT_NONE);

    dwt_readrxdata(final, sizeof(final), 0);
    dwt_writesysstatuslo(status & SYS_STATUS_ALL_RX_GOOD);

    if (!uwb_frame_matches(final, UWB_FRAME_TYPE_FINAL, seq)) {
        ESP_LOGW(TAG, "unexpected FINAL frame type=0x%02x seq=%u expected=%u",
                 final[UWB_FRAME_IDX_TYPE],
                 final[UWB_FRAME_IDX_SEQ],
                 seq);
        uwb_poc_diag_inc_rx_error();
        uwb_poc_ranging_set_invalid();
        return;
    }

    const uint32_t poll_tx_ts = uwb_frame_get_u32(final, UWB_FRAME_IDX_POLL_TX_TS);
    const uint32_t resp_rx_ts = uwb_frame_get_u32(final, UWB_FRAME_IDX_RESP_RX_TS);
    const uint32_t final_tx_ts = uwb_frame_get_u32(final, UWB_FRAME_IDX_FINAL_TX_TS);

    const double ra = (double)(uint32_t)(resp_rx_ts - poll_tx_ts);
    const double rb = (double)(uint32_t)(final_rx_ts - resp_tx_ts);
    const double da = (double)(uint32_t)(final_tx_ts - resp_rx_ts);
    const double db = (double)(uint32_t)(resp_tx_ts - poll_rx_ts);
    const double denominator = ra + rb + da + db;

    if (denominator <= 0.0) {
        uwb_poc_ranging_set_invalid();
        return;
    }

    const double tof_dtu = ((ra * rb) - (da * db)) / denominator;
    const float distance_m = (float)(tof_dtu * DWT_TIME_UNITS * UWB_SPEED_OF_LIGHT_MPS);

    if (distance_m < 0.0f || distance_m > 200.0f) {
        ESP_LOGW(TAG, "invalid range %.2f m", distance_m);
        uwb_poc_ranging_set_invalid();
        return;
    }

    ESP_LOGI(TAG, "distance computed tag=%u anchor=%u distance=%.2f m",
             tag_id,
             UWB_POC_ANCHOR_NODE_ID,
             distance_m);
    uwb_poc_ranging_set_latest(distance_m, true);
}

static void uwb_poc_ranging_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "UWB task started role=%d mode=%d", s_ranging_role, s_ranging_mode);
    uwb_poc_diag_set_mode(s_ranging_mode);

    uint8_t seq = 0;
    while (!s_ranging_stop_requested) {
        switch (s_ranging_mode) {
        case UWB_POC_MODE_SIMPLE_TX:
            uwb_simple_tx_step(&seq);
            break;
        case UWB_POC_MODE_SIMPLE_RX:
            uwb_simple_rx_step();
            break;
        case UWB_POC_MODE_DS_TWR:
        default:
            if (s_ranging_role == UWB_ROLE_TAG) {
                uwb_tag_step(&seq);
            } else {
                uwb_anchor_step();
            }
            break;
        }
    }

    dwt_forcetrxoff();
    s_ranging_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t uwb_poc_ranging_start(uwb_role_t role)
{
    if (role != UWB_ROLE_TAG && role != UWB_ROLE_ANCHOR) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!uwb_poc_range_ensure_mutex()) {
        return ESP_ERR_NO_MEM;
    }

    uwb_poc_range_lock();
    memset(&s_latest_range, 0, sizeof(s_latest_range));
    s_latest_range.anchor_id = UWB_POC_ANCHOR_NODE_ID;
    s_latest_range.valid = false;
    uwb_poc_range_unlock();

    if (s_ranging_task_handle != NULL) {
        return ESP_OK;
    }

    s_ranging_role = role;
    s_ranging_mode = UWB_POC_DEFAULT_MODE;
    s_ranging_stop_requested = false;
    uwb_poc_diag_set_mode(s_ranging_mode);

    BaseType_t created = xTaskCreate(uwb_poc_ranging_task,
                                     "uwb_range",
                                     UWB_POC_RANGING_TASK_STACK_BYTES,
                                     NULL,
                                     UWB_POC_RANGING_TASK_PRIORITY,
                                     &s_ranging_task_handle);
    if (created != pdPASS) {
        s_ranging_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UWB task started role=%d mode=%d", s_ranging_role, s_ranging_mode);
    return ESP_OK;
}

esp_err_t uwb_poc_ranging_stop(void)
{
    if (s_ranging_task_handle == NULL) {
        return ESP_OK;
    }

    s_ranging_stop_requested = true;

    for (int i = 0; i < 20 && s_ranging_task_handle != NULL; ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (s_ranging_task_handle != NULL) {
        ESP_LOGW(TAG, "ranging task did not stop before timeout");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "ranging task stopped");
    return ESP_OK;
}

esp_err_t uwb_poc_ranging_get_latest(uwb_range_t *range)
{
    if (range == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!uwb_poc_range_ensure_mutex()) {
        return ESP_ERR_NO_MEM;
    }

    uwb_poc_range_lock();
    *range = s_latest_range;
    uwb_poc_range_unlock();
    return ESP_OK;
}

esp_err_t uwb_poc_get_latest_range(uwb_range_t *range)
{
    return uwb_poc_ranging_get_latest(range);
}
