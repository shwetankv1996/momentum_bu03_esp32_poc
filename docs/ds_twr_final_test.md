# DS-TWR Final Test

This test should be run only after the SIMPLE_TX / SIMPLE_RX RF link test is passing with sequential packet reception.

## Board configuration

Flash one board as the tag:

```c
#define UWB_POC_DEFAULT_ROLE UWB_ROLE_TAG
#define UWB_POC_DEFAULT_MODE UWB_POC_MODE_DS_TWR
```

Flash the other board as the anchor:

```c
#define UWB_POC_DEFAULT_ROLE UWB_ROLE_ANCHOR
#define UWB_POC_DEFAULT_MODE UWB_POC_MODE_DS_TWR
```

The default node IDs are:

```c
#define UWB_POC_TAG_NODE_ID 1
#define UWB_POC_ANCHOR_NODE_ID 2
```

## Expected log sequence

On the tag:

```text
TAG POLL tx seq=N
TAG RESP rx seq=N
TAG FINAL schedule seq=N
TAG FINAL tx seq=N
```

On the anchor:

```text
ANCHOR POLL rx seq=N
ANCHOR RESP tx seq=N
ANCHOR FINAL rx seq=N
distance computed tag=1 anchor=2 seq=N distance=...
```

## Debug interpretation

If the anchor does not show `ANCHOR POLL rx`, the issue is still first-frame reception or board role/config mismatch.

If the anchor shows `ANCHOR POLL rx` and `ANCHOR RESP tx`, but the tag does not show `TAG RESP rx`, debug the response timing path: `UWB_RX_AFTER_TX_DELAY_UUS`, `UWB_RX_TIMEOUT_UUS`, and `DWT_RESPONSE_EXPECTED` behavior.

If the tag shows `TAG FINAL tx`, but the anchor does not show `ANCHOR FINAL rx`, debug the delayed final transmission path and final RX timeout.

If `ANCHOR FINAL rx` appears but distance is invalid, debug timestamp extraction, antenna delays, and DS-TWR math.

## Notes

- STS remains disabled for the first functional ranging validation.
- Antenna delays are still not calibrated, so initial distance values may be offset even after the packet sequence works.
- The anchor now keeps RX open while waiting for the initial POLL, then uses the tighter DS-TWR timeout only after RESP transmission.
