# ESP32 Bluetooth speaker

ESP32 receives music over Bluetooth (A2DP) and plays it through a MAX98357A I2S amplifier, with a potentiometer for volume.

## Wiring

| MAX98357A | ESP32 |
|---|---|
| DIN | GPIO25 |
| BCLK | GPIO33 |
| LRC | GPIO32 |
| SD | VIN (amp enabled) |
| VIN / GND | 5V rail / common GND |

10k potentiometer: outer legs to 3V3 and GND, wiper to GPIO34.

## Build & flash

```
pio run -t upload
pio device monitor
```

Pair the phone with **ESP32 Speaker**.

`test-sketches/` holds the I2S tone tests used to bring up the amplifier; copy one over `src/main.cpp` to use it.
