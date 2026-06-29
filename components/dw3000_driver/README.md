# DW3000 Driver Component

The PRD requires the Qorvo/DW3000 driver to remain isolated in this component.

The selected ESP-IDF-compatible driver has been cloned into `upstream/` from:

```text
https://github.com/br101/dw3000-decadriver-source.git
```

This component compiles the upstream DW3000 core driver files and provides a local Momentum adapter in:

```text
source/dw3000_momentum_port.c
```

The adapter exports `dw3000_probe_interf` and routes Qorvo SPI callbacks to `components/uwb_port`. Keep platform hooks such as `writetospi`, `readfromspi`, `deca_sleep`, and SPI speed controls in `components/uwb_port`.
