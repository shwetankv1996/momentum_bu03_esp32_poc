# Momentum BU03 ESP32 POC

This firmware brings up the Ai-Thinker BU03 UWB module using ESP32 over SPI.

## Scope

- ESP32 host MCU
- BU03 over SPI
- DW3000 driver integration
- Device ID read
- DW3000 initialization
- Basic DS-TWR ranging
- Single tag + single anchor only

## Not In Scope

- AT commands
- BU03-Kit UART workflow
- Multi-anchor localization
- ROS / cloud / robot navigation integration
- Production calibration flow

## Build

```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

## Role Selection

Set `UWB_POC_DEFAULT_ROLE` in:

```text
components/uwb_poc/include/uwb_poc_config.h
```

Use:

- `UWB_ROLE_TAG`
- `UWB_ROLE_ANCHOR`

Flash one board as tag and another as anchor.

## First Bring-Up Gate

The first gate is successful device ID read from BU03/DW3000 over SPI.

Expected log:

```text
UWB POC: device id = 0xXXXXXXXX
```

The Qorvo-compatible DW3000 driver is vendored under `components/dw3000_driver/upstream`. The Momentum adapter binds the driver probe interface to `components/uwb_port`.

## Second Bring-Up Gate

The second gate is successful DS-TWR ranging between two boards.

## DW3000 Driver Integration

The selected Qorvo/DW3000 driver source is located under:

```text
components/dw3000_driver/
```

The integration entry points are:

- `components/dw3000_driver/CMakeLists.txt`
- `components/dw3000_driver/source/dw3000_momentum_port.c`
- `components/uwb_poc/source/uwb_poc_config.c`
- `components/uwb_poc/source/uwb_poc_ranging.c`

Keep raw `dwt_*` usage inside `components/uwb_poc` and `components/uwb_port`; `main/app_main.c` must only use the public `uwb_poc` API.
