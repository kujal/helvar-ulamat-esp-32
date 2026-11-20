# ESP32 Bluetooth speaker with FM radio

ESP32 receives music over Bluetooth (A2DP) and plays it through a PAM8403 amplifier. A CD4051B analog switch selects between Bluetooth and an RDA5807 FM receiver.

See [WIRING.md](WIRING.md) for the full schematic, the parts list and the reasoning behind each component.

## Status

| Part | State |
|---|---|
| Bluetooth → ESP32 built-in 8-bit DAC (GPIO25) → PAM8403 | working, but hissy and coarse (`src/main.cpp`) |
| Bluetooth → PCM5102 I2S DAC | planned, replaces the 8-bit DAC |
| RDA5807 FM receiver | wiring documented, firmware not yet written |
| CD4051B source switch | wiring documented |

The current firmware writes the DAC from a 44.1 kHz timer interrupt. The DMA-driven DAC mode produced static even when it was outputting silence.

## Build & flash

```
pio run -t upload
pio device monitor
```

Pair the phone with **ESP32 Speaker**.

## Test sketches

`test-sketches/` holds the firmware used while bringing up the hardware. To use one, copy it over `src/main.cpp`.

| Sketch | Purpose |
|---|---|
| `i2s_beep_test.cpp` | 440/880 Hz beeps to a MAX98357A over I2S |
| `i2s_tone_pin_monitor.cpp` | continuous I2S tone, and checks that the I2S pins toggle |
| `bluetooth_dac_diag.cpp` | Bluetooth to the built-in DAC through the audio library's DMA mode, with an idle test tone |

The MAX98357A version of the Bluetooth speaker, with a volume pot on GPIO34, is in commit `decef96`.
