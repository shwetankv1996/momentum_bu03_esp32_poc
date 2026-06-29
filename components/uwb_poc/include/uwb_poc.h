#pragma once

#include "esp_err.h"
#include "uwb_poc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t uwb_poc_init(void);
esp_err_t uwb_poc_set_role(uwb_role_t role);
esp_err_t uwb_poc_start(void);
esp_err_t uwb_poc_stop(void);
esp_err_t uwb_poc_get_latest_range(uwb_range_t *range);
esp_err_t uwb_poc_get_diag(uwb_diag_t *diag);

#ifdef __cplusplus
}
#endif
