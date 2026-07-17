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
 * Test selection:
 * - SIMPLE_TX / SIMPLE_RX are retained for RF bring-up regression.
 * - DS_TWR is the final ranging test path after SIMPLE_RX receives clean
 *   sequential frames from SIMPLE_TX.
 *
 * For final DS-TWR testing, flash one board as:
 *   UWB_ROLE_TAG + UWB_POC_MODE_DS_TWR
 * and the other board as:
 *   UWB_ROLE_ANCHOR + UWB_POC_MODE_DS_TWR
 */
#define UWB_POC_DEFAULT_ROLE UWB_ROLE_ANCHOR
#define UWB_POC_DEFAULT_MODE UWB_POC_MODE_DS_TWR

/*
 * Keep this as a C expression instead of #if on enum values. The preprocessor
 * treats unknown enum identifiers as 0, which made ANCHOR builds print node_id=1.
 */
#define UWB_POC_DEFAULT_NODE_ID \
    ((UWB_POC_DEFAULT_ROLE == UWB_ROLE_TAG) ? UWB_POC_TAG_NODE_ID : UWB_POC_ANCHOR_NODE_ID)

#define UWB_POC_DEFAULT_CHANNEL 5
#define UWB_POC_DEFAULT_DATA_RATE 6800

/* Keep STS off for first DS-TWR functional validation. Re-enable after baseline ranging works. */
#define UWB_POC_ENABLE_STS 0

#define UWB_POC_RANGING_TASK_STACK_BYTES 4096
#define UWB_POC_RANGING_TASK_PRIORITY 5
#define UWB_POC_DIAG_PRINT_PERIOD_MS 1000

/*
 * Keep retry latency near zero during bring-up. Individual modes now control
 * their own hardware and host-side RX wait windows.
 */
#define UWB_POC_DEBUG_STEP_DELAY_MS 0

#define UWB_POC_SIMPLE_TX_PERIOD_MS 1000
