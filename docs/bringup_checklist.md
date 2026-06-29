# BU03 Bring-Up Checklist

## Electrical

- 3.3 V rail verified
- GND common with ESP32
- SPI lines verified
- RSTN verified
- IRQ verified
- antenna keep-out checked

## SPI

- SPI mode verified
- CS polarity verified
- conservative SPI clock used first
- device ID read successful
- repeated device ID reads stable

## Driver

- hardware reset works
- DW3000 initialization works
- default PHY config applied
- no repeated SPI errors

## Ranging

- simple TX/RX works
- DS-TWR tag/anchor flow works
- distance appears at fixed test distances
- invalid readings are flagged
