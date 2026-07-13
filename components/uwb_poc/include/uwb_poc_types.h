#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UWB_ROLE_TAG = 0,
    UWB_ROLE_ANCHOR = 1,
} uwb_role_t;

typedef enum {
    UWB_POC_MODE_SIMPLE_TX = 0,
    UWB_POC_MODE_SIMPLE_RX,
    UWB_POC_MODE_DS_TWR,
} uwb_poc_mode_t;

typedef enum {
    UWB_POC_STATE_IDLE = 0,
    UWB_POC_STATE_INITIALIZED,
    UWB_POC_STATE_RUNNING,
    UWB_POC_STATE_ERROR,
} uwb_poc_state_t;

typedef struct {
    uint16_t node_id;
    uwb_role_t role;
    uint8_t channel;
    uint8_t data_rate;
} uwb_node_config_t;

typedef struct {
    uint16_t anchor_id;
    float distance_m;
    uint32_t timestamp_ms;
    bool valid;
} uwb_range_t;

typedef struct {
    bool spi_ok;
    bool gpio_ok;
    bool irq_ok;
    bool device_detected;
    uint32_t device_id;
    uwb_poc_state_t state;
    uwb_role_t role;
    uwb_poc_mode_t mode;
    uint32_t init_count;
    uint32_t reset_count;
    uint32_t tx_count;
    uint32_t rx_ok_count;
    uint32_t rx_timeout_count;
    uint32_t rx_error_count;
    uint32_t simple_tx_count;
    uint32_t simple_rx_ok_count;
    uint32_t simple_rx_timeout_count;
    uint32_t simple_rx_error_count;
    uint32_t last_rx_status;
    uint32_t range_ok_count;
    uint32_t range_invalid_count;
    uint32_t last_range_ms;
    float last_range_m;
} uwb_diag_t;
