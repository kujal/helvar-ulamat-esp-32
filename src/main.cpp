#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// MAX98357A wiring
const int I2S_BCLK = 33;  // -> BCLK
const int I2S_LRC = 32;   // -> LRC
const int I2S_DOUT = 25;  // -> DIN
// 10k pot: ends to 3V3 and GND, wiper to GPIO34 (ADC1, input-only, usable alongside Bluetooth)
const int VOLUME_POT_PIN = 34;

const char *BT_NAME = "ESP32 Speaker";

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

void onConnectionState(esp_a2d_connection_state_t state, void *) {
  Serial.printf("Bluetooth: %s\n", a2dp_sink.to_str(state));
}

void onAudioState(esp_a2d_audio_state_t state, void *) {
  Serial.printf("Audio: %s\n", a2dp_sink.to_str(state));
}

void setup() {
  Serial.begin(115200);

  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = I2S_BCLK;
  cfg.pin_ws = I2S_LRC;
  cfg.pin_data = I2S_DOUT;
  i2s.begin(cfg);

  a2dp_sink.set_on_connection_state_changed(onConnectionState);
  a2dp_sink.set_on_audio_state_changed(onAudioState);
  a2dp_sink.start(BT_NAME);
  Serial.printf("Bluetooth speaker \"%s\" ready, pair from your phone\n", BT_NAME);
}

// Pot only overrides the volume when it is actually turned,
// so phone volume buttons keep working in between
void updateVolumeFromPot() {
  static int lastVolume = -1;
  long sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(VOLUME_POT_PIN);
  int volume = map(sum / 16, 0, 4095, 0, 127);
  if (lastVolume < 0 || abs(volume - lastVolume) >= 2) {
    lastVolume = volume;
    a2dp_sink.set_volume(volume);
    Serial.printf("Volume: %d / 127\n", volume);
  }
}

void loop() {
  updateVolumeFromPot();
  delay(50);
}
