#pragma once

#include "driver/spi_master.h"
#include "uwb_poc_types.h"

/*
 * Update these pins based on actual Momentum Robotics PCB.
 * These are placeholders and must be verified against the schematic.
 */
#define UWB_POC_SPI_HOST SPI2_HOST
#define UWB_POC_PIN_MOSI 23
#define UWB_POC_PIN_MISO 19
#define UWB_POC_PIN_SCLK 18
#define UWB_POC_PIN_CS 5
#define UWB_POC_PIN_RST 27
#define UWB_POC_PIN_IRQ 34

#define UWB_POC_SPI_CLOCK_HZ (8 * 1000 * 1000)

#define UWB_POC_TAG_NODE_ID 1
#define UWB_POC_ANCHOR_NODE_ID 2

/*
 * Debug selection:
 * - For first RF bring-up, flash one board with SIMPLE_TX + TAG
 *   and the other with SIMPLE_RX + ANCHOR.
 * - Return to DS_TWR only after SIMPLE_RX receives frames.
 */
#define UWB_POC_DEFAULT_ROLE UWB_ROLE_ANCHOR
#define UWB_POC_DEFAULT_MODE UWB_POC_MODE_SIMPLE_RX

#if UWB_POC_DEFAULT_ROLE == UWB_ROLE_TAG
#define UWB_POC_DEFAULT_NODE_ID UWB_POC_TAG_NODE_ID
#else
#define UWB_POC_DEFAULT_NODE_ID UWB_POC_ANCHOR_NODE_ID
#endif

#define UWB_POC_DEFAULT_CHANNEL 5
#define UWB_POC_DEFAULT_DATA_RATE 6800

/* Keep STS off for basic TX/RX debug. Re-enable for DS-TWR STS validation later. */
#define UWB_POC_ENABLE_STS 0

#define UWB_POC_RANGING_TASK_STACK_BYTES 4096
#define UWB_POC_RANGING_TASK_PRIORITY 5
#define UWB_POC_DIAG_PRINT_PERIOD_MS 1000

/*
 * Keep SIMPLE_RX re-arm latency near zero during RF bring-up.
 * The previous 500 ms delay made the receiver deaf between RX windows and
 * caused it to catch only occasional SIMPLE_TX frames.
 */
#define UWB_POC_DEBUG_STEP_DELAY_MS 0

#define UWB_POC_SIMPLE_TX_PERIOD_MS 1000
