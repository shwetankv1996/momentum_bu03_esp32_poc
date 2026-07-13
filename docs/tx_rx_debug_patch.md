# TX/RX Debug Patch

This branch adds a temporary simple TX/RX debug path before DS-TWR ranging.

## Why

The current logs showed that both ESP32 boards can read the DW3000 device ID and initialize the driver, but the anchor never receives the first tag poll. That means the next useful gate is basic RF packet reception, not distance calculation.

## How to use

Edit `components/uwb_poc/include/uwb_poc_config.h` before flashing each board.

### TX board

```c
#define UWB_POC_DEFAULT_ROLE UWB_ROLE_TAG
#define UWB_POC_DEFAULT_MODE UWB_POC_MODE_SIMPLE_TX
```

### RX board

```c
#define UWB_POC_DEFAULT_ROLE UWB_ROLE_ANCHOR
#define UWB_POC_DEFAULT_MODE UWB_POC_MODE_SIMPLE_RX
```

Keep STS disabled for this first RF debug gate:

```c
#define UWB_POC_ENABLE_STS 0
```

Expected TX log:

```text
SIMPLE_TX sent seq=1
```

Expected RX log:

```text
SIMPLE_RX ok raw=4d 55 55 01 ...
```

If RX still times out, inspect `last_status` in the app diagnostic log and the per-RX status log printed by `UWB_RANGE`.

Return to DS-TWR only after simple RX works.
