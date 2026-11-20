# Wiring overview

ESP32 Bluetooth speaker with FM radio (mono, one speaker).

| Part | Role |
|---|---|
| ESP32 DevKit | Bluetooth (A2DP) receiver, I2S audio out, controls the radio and the source switch |
| PCM5102 | I2S DAC: 16-bit Bluetooth audio → analog |
| RDA5807 | FM receiver, controlled over I2C |
| CD4051B | 8-channel analog switch: picks Bluetooth or FM |
| PAM8403 | Class-D amplifier → speaker |
| AMS1117-3.3 | 5 V → 3.3 V for the RDA5807, the CD4051B, the pull-ups and the bias networks |

The CD4051B switches a single signal, so the system is mono:
- **The firmware sends a mono mix** (L = R) to the PCM5102, and only **LOUT** is used.
- **The RDA5807 runs in mono mode** (L = R), and only its **L** output is used.

## Signal flow

```
 phone ))) Bluetooth ─→ ESP32 ─ I2S ─→ PCM5102 LOUT ─┐
                          │                          ├─→ CD4051B ─→ PAM8403 L ─→ speaker
                          └─ I2C ─→ RDA5807 L ───────┘      ↑
                          └─ GPIO16/19 (select + mute) ─────┘
```

## Power

```
 5V supply (≥1 A) ──┬──→ PAM8403 5V+        (470–1000 µF + 100 nF across 5V/GND at the module)
                    ├──→ ESP32 VIN
                    ├──→ PCM5102 VIN        (module has its own regulator)
                    └──→ AMS1117 IN ──→ 3.3V rail ──┬──→ RDA5807 VCC
                                                    ├──→ CD4051B VDD (pin 16)
                                                    ├──→ I2C pull-ups (2 × 4.7k)
                                                    └──→ bias networks (100k)
 GND: one common ground for everything, joined at a single point near the PAM8403.
```

- **While the ESP32 is on USB for flashing,** disconnect the 5V supply from ESP32 VIN, but keep the grounds joined. Two supplies on one pin fight each other.
- **Never connect the AMS1117's 3.3V to the ESP32's 3V3 pin** while USB or VIN also powers the ESP32. This crashed the USB connection during testing.
- **The RDA5807 maximum is 3.6 V,** so it only ever gets the 3.3V rail.

## Overall diagram

```
   ┌──────────────────────┐           ┌──────────────────┐
   │ ESP32                │           │ PCM5102          │
   │              GPIO33 ●┼───────────┼● BCK             │
   │              GPIO32 ●┼───────────┼● LCK             │
   │              GPIO25 ●┼───────────┼● DIN             │
   │                      │     GND ──┼● SCK  (internal clock)
   │                      │           │  LOUT ●───┐      │
   │                      │           └───────────┼──────┘
   │                      │                       │  input network
   │              GPIO21 ●┼─┬─ SDA ┐              │  ─┤1µF├─┬─→ pin
   │              GPIO22 ●┼─┼┬ SCL ┤ RDA5807      │         ├─100k─ 3.3V
   │                      │ ││     │  L ●──┐      │         └─100k─ GND
   │                      │ 4.7k × 2  ANT ← ~75 cm wire
   │                      │ to 3.3V└───────┼──────┼──────┐
   │                      │                │      │      │
   │                      │     ┌──────────┼──────┼──────┼───────────────┐
   │                      │     │ CD4051B  │      │      │               │
   │                      │     │     (13) CH0 ●──┘  ←───┘ (via network) │
   │                      │     │     (14) CH1 ●──── RDA L (via network) │
   │              GPIO16 ●┼─────┼● A (11)                                │
   │                 GND ─┼─────┼● B (10), C (9)                         │
   │              GPIO19 ●┼─────┼● INH (6)                               │
   │                      │     │  COM (3) ●───┐                         │
   └──────────────────────┘     └──────────────┼─────────────────────────┘
                                               │ output network
                                               └─┤1µF├─ 10k ─┬─→ PAM8403 L-IN
                                                             └─ 1k ── GND
                                ┌──────────────────────────────────┐
                                │ PAM8403  L± ●──● ─→ speaker      │
                                └──────────────────────────────────┘
```

## Connections

### ESP32 ↔ PCM5102 (I2S)

| ESP32 | PCM5102 | Notes |
|---|---|---|
| GPIO33 | BCK | bit clock |
| GPIO32 | LCK | left/right (word) clock |
| GPIO25 | DIN | audio data |
| – | SCK | **to GND**: the PCM5102 then makes its own clock from BCK |
| 5V | VIN | |
| GND | GND | |
| – | LOUT | input network → CD4051B CH0 |
| – | ROUT | not used (same signal as LOUT) |

**Solder jumpers on the back of the purple PCM5102 module:**

| Jumper | Setting | Meaning |
|---|---|---|
| H1L FLT | L | normal filter |
| H2L DEMP | L | de-emphasis off |
| H3L XSMT | **H** | soft-mute off. If it's set to L, the DAC stays muted. |
| H4L FMT | L | I2S format |

### ESP32 ↔ RDA5807 (I2C)

| ESP32 | RDA5807 | Notes |
|---|---|---|
| GPIO21 | SDA | 4.7k to the 3.3V rail |
| GPIO22 | SCK / SCL | 4.7k to the 3.3V rail |
| – | VCC | 3.3V rail from the AMS1117 |
| GND | GND | |
| – | ANT | wire about 75 cm long |
| – | L | input network → CD4051B CH1 |
| – | R | not used (mono mode) |

**Pull-ups go to 3.3 V only, never to GND.**

### CD4051B (source switch)

Pin numbers as in the TI datasheet (Figure 4-1, top view). Pin 1 is next to the notch.

| Pin | Name | Connect to |
|---|---|---|
| 16 | VDD | 3.3V rail. 3.3 V logic from the ESP32 is then a valid HIGH, and the signals must stay within 0–3.3 V. |
| 8 | VSS | GND |
| 7 | VEE | GND |
| 11 | A | GPIO16 |
| 10 | B | GND |
| 9 | C | GND |
| 6 | INH | GPIO19 (HIGH = everything off, used as mute) |
| 13 | CH0 | PCM5102 LOUT (through the input network) |
| 14 | CH1 | RDA5807 L (through the input network) |
| 15, 12, 1, 5, 2, 4 | CH2, CH3, CH4, CH5, CH6, CH7 | GND (unused) |
| 3 | COM | output network → PAM8403 L-IN |

| A (GPIO16) | INH (GPIO19) | Source |
|---|---|---|
| LOW | LOW | CH0: Bluetooth (PCM5102) |
| HIGH | LOW | CH1: FM (RDA5807) |
| x | HIGH | muted |

### Input network: one per source (2 in total)

```
source ──┤1µF├──┬──→ CD4051B CH0 / CH1
                ├── 100k ── 3.3V
                └── 100k ── GND
```

The capacitor blocks the source's own DC voltage, and the two 100k resistors centre the signal at 1.65 V. The CD4051B only passes signals between 0 V and its 3.3 V supply, so audio swinging around 0 V would be cut off at the bottom.

**PCM5102 level:** its full-scale output is about 6 Vpp, which doesn't fit the 0–3.3 V switching range. The firmware caps the digital level at about 40%, which gives about 2.4 Vpp. With a 16-bit DAC this costs no audible quality, unlike the ESP32's 8-bit DAC.

### Output network

```
COM ──┤1µF├── 10k ──┬──→ PAM8403 L-IN
                    └── 1k ── GND
```

The capacitor removes the 1.65 V bias again. The 10k/1k divider attenuates about 11×, because the PAM8403 amplifies about 16× and would otherwise distort. The 1k also keeps the amplifier input at low impedance, so it picks up less noise.

### PAM8403

| Pin | Connect to |
|---|---|
| 5V+ | 5V supply, with 470–1000 µF and 100 nF across 5V/GND at the module |
| − | GND |
| G (input ground) | GND |
| L-IN | output network from COM |
| R-IN | GND (unused) |
| L+ / L− | speaker. Neither speaker wire may touch GND. |

On this PAM8403 module, the output pair that matches the input channel was the one that worked. Check both pairs if there's no sound.

## ESP32 pin summary

| Pin | Use |
|---|---|
| GPIO25 | I2S DIN → PCM5102 |
| GPIO32 | I2S LCK → PCM5102 |
| GPIO33 | I2S BCK → PCM5102 |
| GPIO21 | I2C SDA → RDA5807 |
| GPIO22 | I2C SCL → RDA5807 |
| GPIO16 | CD4051B A (source select) |
| GPIO19 | CD4051B INH (mute) |
| GPIO34 | free, for an optional volume pot (wiper; outer legs to 3.3V and GND) |
| VIN | 5V supply |

## Parts

| Part | Qty | Where |
|---|---|---|
| 1 µF capacitor (ceramic or film) | 3 | 2 input networks + 1 output network |
| 100 kΩ | 4 | bias, 2 per input network |
| 10 kΩ | 1 | output divider |
| 1 kΩ | 1 | output divider |
| 4.7 kΩ | 2 | I2C pull-ups |
| 470–1000 µF electrolytic + 100 nF | 1 + 1 | PAM8403 supply decoupling |

## Lessons from testing

- **ESP32 built-in 8-bit DAC:** too noisy and coarse for music, so the PCM5102 replaces it. That frees the analog path from DAC static and quantization distortion.
- **Ground loop:** the ESP32 on the Mac's USB and the PAM8403 on a separate supply gave loud static. Use one supply and one common ground.
- **Bluetooth radio bursts** make the 5V supply dip rhythmically, which sounds like knocking. Hence the large capacitor at the PAM8403.
- **RDA5807 on a bare 2 mm-pad module** answered I2C only intermittently. It needs solid solder joints or a breakout board.
- **Unused wires** act as antennas, so remove them, and tie the switch's unused inputs to GND.
