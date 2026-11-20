#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// Bluetooth speaker: phone -> A2DP -> ESP32 built-in 8-bit DAC (GPIO25)
//   -> CD4051B CH0 -> 1µF -> PAM8403 -> speaker
const int MUX_A_PIN = 16;    // CD4051B A/B/C all LOW = CH0 (ESP32 DAC)
const int MUX_B_PIN = 17;
const int MUX_C_PIN = 18;
const int MUX_INH_PIN = 19;  // CD4051B INH: LOW = enabled

// Full-scale DAC output (~3.3 Vpp) overdrives the PAM8403 (~16x gain), so cap the level.
// Raising this and attenuating in hardware instead (e.g. 10k/1k divider) gives less hiss.
const float MAX_LEVEL = 0.1f;

const char *BT_NAME = "ESP32 Speaker";
const int SAMPLE_RATE = 44100;

AnalogAudioStream dac;

// Stereo 16-bit from A2DP -> mono, level-scaled, to DAC channel 1 (GPIO25) only.
// The stereo DAC mode alternates channels at the base rate, which halves the
// per-channel rate and garbles the audio, so the DAC runs mono.
class MonoDacOutput : public AudioStream {
 public:
  volatile uint32_t lastWriteMs = 0;

  size_t write(const uint8_t *data, size_t len) override {
    const int16_t *in = (const int16_t *)data;
    size_t frames = len / 4;
    while (frames > 0) {
      size_t n = min(frames, (size_t)BLOCK);
      for (size_t i = 0; i < n; i++) {
        int32_t mono = ((int32_t)in[2 * i] + in[2 * i + 1]) / 2;
        buf[i] = (int16_t)(mono * MAX_LEVEL);
      }
      writeAll(buf, n);
      in += 2 * n;
      frames -= n;
    }
    lastWriteMs = millis();
    return len;
  }

  // Diagnostic: while idle, play a 440 Hz sine at the same level through the same path.
  // Clean tone + Bluetooth on -> static comes from the streamed data; noisy tone -> analog/RF.
  void writeIdleTone() {
    static float phase = 0;
    for (size_t i = 0; i < BLOCK; i++) {
      buf[i] = (int16_t)(32767 * MAX_LEVEL * sinf(phase));
      phase += 2 * PI * 440 / SAMPLE_RATE;
      if (phase >= 2 * PI) phase -= 2 * PI;
    }
    writeAll(buf, BLOCK);
  }

  uint32_t droppedBytes = 0;

 private:
  static const size_t BLOCK = 256;
  int16_t buf[BLOCK];

  // dac.write can accept less than requested; keep going so no samples are dropped
  void writeAll(int16_t *samples, size_t count) {
    uint8_t *p = (uint8_t *)samples;
    size_t left = count * 2;
    int tries = 0;
    while (left > 0 && tries < 50) {
      size_t done = dac.write(p, left);
      if (done == 0) {
        tries++;
        delay(1);
        continue;
      }
      p += done;
      left -= done;
    }
    droppedBytes += left;
  }
};

MonoDacOutput output;
BluetoothA2DPSink a2dp_sink(output);

void onConnectionState(esp_a2d_connection_state_t state, void *) {
  Serial.printf("Bluetooth: %s\n", a2dp_sink.to_str(state));
}

void onAudioState(esp_a2d_audio_state_t state, void *) {
  Serial.printf("Audio: %s\n", a2dp_sink.to_str(state));
}

void setup() {
  Serial.begin(115200);

  for (int pin : {MUX_A_PIN, MUX_B_PIN, MUX_C_PIN, MUX_INH_PIN}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  auto cfg = dac.defaultConfig(TX_MODE);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.channels = 1;  // DAC channel 1 = GPIO25
  cfg.bits_per_sample = 16;
  dac.begin(cfg);

  a2dp_sink.set_on_connection_state_changed(onConnectionState);
  a2dp_sink.set_on_audio_state_changed(onAudioState);
  a2dp_sink.start(BT_NAME);
  Serial.printf("Bluetooth speaker \"%s\" ready, pair from your phone\n", BT_NAME);
}

void loop() {
  static uint32_t lastReport = 0;
  if (millis() - output.lastWriteMs > 100) {
    output.writeIdleTone();  // blocks ~6 ms while the DMA plays it out
  } else {
    delay(10);
  }
  if (millis() - lastReport > 2000) {
    lastReport = millis();
    Serial.printf("%s, dropped %u bytes\n", millis() - output.lastWriteMs > 100 ? "idle tone" : "streaming",
                  output.droppedBytes);
  }
}
