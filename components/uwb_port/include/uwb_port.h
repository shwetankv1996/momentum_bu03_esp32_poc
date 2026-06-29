#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t uwb_port_init(void);
esp_err_t uwb_port_spi_init(void);
esp_err_t uwb_port_gpio_init(void);
esp_err_t uwb_port_irq_init(void);

void uwb_port_reset(void);
bool uwb_port_irq_is_asserted(void);
void uwb_port_sleep_ms(uint32_t delay_ms);
uint32_t uwb_port_get_time_ms(void);

/*
 * DW3000/Qorvo driver platform hooks. If the imported driver uses different
 * names or types, add adapter wrappers here instead of exposing SPI to main.
 */
int writetospi(uint16_t headerLength,
               const uint8_t *headerBuffer,
               uint16_t bodyLength,
               const uint8_t *bodyBuffer);

int readfromspi(uint16_t headerLength,
                const uint8_t *headerBuffer,
                uint16_t readLength,
                uint8_t *readBuffer);

int writetospiwithcrc(uint16_t headerLength,
                      const uint8_t *headerBuffer,
                      uint16_t bodyLength,
                      const uint8_t *bodyBuffer,
                      uint8_t crc8);

void deca_sleep(unsigned int time_ms);
void port_set_dw_ic_spi_slowrate(void);
void port_set_dw_ic_spi_fastrate(void);

#ifdef __cplusplus
}
#endif
