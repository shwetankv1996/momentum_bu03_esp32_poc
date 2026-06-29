#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "uwb_poc_types.h"

esp_err_t uwb_poc_driver_init(void);
esp_err_t uwb_poc_read_device_id(uint32_t *device_id);
esp_err_t uwb_poc_apply_default_config(void);

esp_err_t uwb_poc_ranging_start(uwb_role_t role);
esp_err_t uwb_poc_ranging_stop(void);
esp_err_t uwb_poc_ranging_get_latest(uwb_range_t *range);

void uwb_poc_diag_reset(void);
void uwb_poc_diag_set_state(uwb_poc_state_t state);
void uwb_poc_diag_set_role(uwb_role_t role);
void uwb_poc_diag_set_device_id(uint32_t device_id);
void uwb_poc_diag_set_spi_ok(bool ok);
void uwb_poc_diag_set_gpio_ok(bool ok);
void uwb_poc_diag_set_irq_ok(bool ok);
void uwb_poc_diag_inc_init(void);
void uwb_poc_diag_inc_reset(void);
void uwb_poc_diag_inc_tx(void);
void uwb_poc_diag_inc_rx_ok(void);
void uwb_poc_diag_inc_rx_timeout(void);
void uwb_poc_diag_inc_rx_error(void);
void uwb_poc_diag_update_range(float distance_m, bool valid);
esp_err_t uwb_poc_diag_get(uwb_diag_t *diag);
