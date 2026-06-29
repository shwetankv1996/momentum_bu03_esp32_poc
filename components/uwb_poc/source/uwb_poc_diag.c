#include "uwb_poc.h"

#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "uwb_poc_config.h"
#include "uwb_poc_internal.h"

static SemaphoreHandle_t s_diag_mutex;
static uwb_diag_t s_diag;

static bool uwb_poc_diag_ensure_mutex(void)
{
    if (s_diag_mutex == NULL) {
        s_diag_mutex = xSemaphoreCreateMutex();
    }

    return s_diag_mutex != NULL;
}

static void uwb_poc_diag_lock(void)
{
    if (uwb_poc_diag_ensure_mutex()) {
        (void)xSemaphoreTake(s_diag_mutex, portMAX_DELAY);
    }
}

static void uwb_poc_diag_unlock(void)
{
    if (s_diag_mutex != NULL) {
        (void)xSemaphoreGive(s_diag_mutex);
    }
}

void uwb_poc_diag_reset(void)
{
    uwb_poc_diag_lock();
    memset(&s_diag, 0, sizeof(s_diag));
    s_diag.state = UWB_POC_STATE_IDLE;
    s_diag.role = UWB_POC_DEFAULT_ROLE;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_set_state(uwb_poc_state_t state)
{
    uwb_poc_diag_lock();
    s_diag.state = state;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_set_role(uwb_role_t role)
{
    uwb_poc_diag_lock();
    s_diag.role = role;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_set_device_id(uint32_t device_id)
{
    uwb_poc_diag_lock();
    s_diag.device_id = device_id;
    s_diag.device_detected = device_id != 0 && device_id != 0xFFFFFFFFU;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_set_spi_ok(bool ok)
{
    uwb_poc_diag_lock();
    s_diag.spi_ok = ok;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_set_gpio_ok(bool ok)
{
    uwb_poc_diag_lock();
    s_diag.gpio_ok = ok;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_set_irq_ok(bool ok)
{
    uwb_poc_diag_lock();
    s_diag.irq_ok = ok;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_inc_init(void)
{
    uwb_poc_diag_lock();
    s_diag.init_count++;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_inc_reset(void)
{
    uwb_poc_diag_lock();
    s_diag.reset_count++;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_inc_tx(void)
{
    uwb_poc_diag_lock();
    s_diag.tx_count++;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_inc_rx_ok(void)
{
    uwb_poc_diag_lock();
    s_diag.rx_ok_count++;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_inc_rx_timeout(void)
{
    uwb_poc_diag_lock();
    s_diag.rx_timeout_count++;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_inc_rx_error(void)
{
    uwb_poc_diag_lock();
    s_diag.rx_error_count++;
    uwb_poc_diag_unlock();
}

void uwb_poc_diag_update_range(float distance_m, bool valid)
{
    uwb_poc_diag_lock();
    s_diag.last_range_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    s_diag.last_range_m = distance_m;
    if (valid) {
        s_diag.range_ok_count++;
    } else {
        s_diag.range_invalid_count++;
    }
    uwb_poc_diag_unlock();
}

esp_err_t uwb_poc_diag_get(uwb_diag_t *diag)
{
    if (diag == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!uwb_poc_diag_ensure_mutex()) {
        return ESP_ERR_NO_MEM;
    }

    uwb_poc_diag_lock();
    *diag = s_diag;
    uwb_poc_diag_unlock();
    return ESP_OK;
}

esp_err_t uwb_poc_get_diag(uwb_diag_t *diag)
{
    return uwb_poc_diag_get(diag);
}
