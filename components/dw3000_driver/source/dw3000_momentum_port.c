#include "deca_device_api.h"
#include "deca_interface.h"

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "rom/ets_sys.h"
#include "uwb_port.h"

static portMUX_TYPE s_dw3000_mutex = portMUX_INITIALIZER_UNLOCKED;

extern const struct dwt_driver_s dw3000_driver;

static int32_t momentum_readfromspi(uint16_t headerLength,
                                    uint8_t *headerBuffer,
                                    uint16_t readLength,
                                    uint8_t *readBuffer)
{
    return readfromspi(headerLength,
                       headerBuffer,
                       readLength,
                       readBuffer) == 0 ? DWT_SUCCESS : DWT_ERROR;
}

static int32_t momentum_writetospi(uint16_t headerLength,
                                   const uint8_t *headerBuffer,
                                   uint16_t bodyLength,
                                   const uint8_t *bodyBuffer)
{
    return writetospi(headerLength,
                      headerBuffer,
                      bodyLength,
                      bodyBuffer) == 0 ? DWT_SUCCESS : DWT_ERROR;
}

static int32_t momentum_writetospiwithcrc(uint16_t headerLength,
                                          const uint8_t *headerBuffer,
                                          uint16_t bodyLength,
                                          const uint8_t *bodyBuffer,
                                          uint8_t crc8)
{
    uint8_t *crc_body = (uint8_t *)malloc((size_t)bodyLength + 1U);
    if (crc_body == NULL) {
        return DWT_ERROR;
    }

    if (bodyLength > 0 && bodyBuffer != NULL) {
        memcpy(crc_body, bodyBuffer, bodyLength);
    }
    crc_body[bodyLength] = crc8;

    int ret = writetospi(headerLength, headerBuffer, bodyLength + 1U, crc_body);
    free(crc_body);

    return ret == 0 ? DWT_SUCCESS : DWT_ERROR;
}

static void momentum_wakeup_device_with_io(void)
{
    /*
     * The POC uses an explicit reset during init. For driver wake-up, let the
     * DW3000 settle without toggling RSTN again.
     */
    uwb_port_sleep_ms(2);
}

static const struct dwt_spi_s s_dw3000_spi = {
    .readfromspi = momentum_readfromspi,
    .writetospi = momentum_writetospi,
    .writetospiwithcrc = momentum_writetospiwithcrc,
    .setslowrate = port_set_dw_ic_spi_slowrate,
    .setfastrate = port_set_dw_ic_spi_fastrate,
};

static const struct dwt_driver_s *s_driver_list[] = {
    &dw3000_driver,
};

const struct dwt_probe_s dw3000_probe_interf = {
    .dw = NULL,
    .spi = (void *)&s_dw3000_spi,
    .wakeup_device_with_io = momentum_wakeup_device_with_io,
    .driver_list = (struct dwt_driver_s **)s_driver_list,
    .dw_driver_num = 1,
};

void wakeup_device_with_io(void)
{
    momentum_wakeup_device_with_io();
}

decaIrqStatus_t decamutexon(void)
{
    portENTER_CRITICAL(&s_dw3000_mutex);
    return 0;
}

void decamutexoff(decaIrqStatus_t s)
{
    (void)s;
    portEXIT_CRITICAL(&s_dw3000_mutex);
}

void deca_usleep(unsigned long time_us)
{
    ets_delay_us((uint32_t)time_us);
}
