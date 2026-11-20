#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "hal/dac_ll.h"

// Bluetooth speaker: phone -> A2DP -> ESP32 built-in 8-bit DAC (GPIO25)
//   -> CD4051B CH0 -> 1µF -> PAM8403 -> speaker
//
// The DAC is written directly from a 44.1 kHz timer interrupt. The DMA-driven
// (dac_continuous) mode produced loud static on GPIO25 even when outputting silence.
const int DAC_PIN = 25;
const int MUX_A_PIN = 16;    // CD4051B A/B/C all LOW = CH0 (ESP32 DAC)
const int MUX_B_PIN = 17;
const int MUX_C_PIN = 18;
const int MUX_INH_PIN = 19;  // CD4051B INH: LOW = enabled

// Full-scale DAC output (~3.3 Vpp) overdrives the PAM8403 (~16x gain), so cap the level.
// Raising this and attenuating in hardware instead (e.g. 10k/1k divider) gives less hiss.
const float MAX_LEVEL = 0.1f;

const char *BT_NAME = "ESP32 Speaker";

// 40 MHz timer / 907 = 44101 Hz, within 0.003% of 44.1 kHz
const uint32_t TIMER_HZ = 40000000;
const uint32_t TICKS_PER_SAMPLE = 907;

// Mono 8-bit samples from the Bluetooth task to the timer interrupt
const size_t RING_SIZE = 8192;  // ~185 ms
volatile uint8_t ring[RING_SIZE];
volatile size_t ringHead = 0;   // written by Bluetooth task
volatile size_t ringTail = 0;   // read by interrupt
volatile uint32_t underruns = 0;

void IRAM_ATTR onSampleTimer() {
  if (ringTail != ringHead) {
    dac_ll_update_output_value(DAC_CHAN_0, ring[ringTail]);
    ringTail = (ringTail + 1) % RING_SIZE;
  } else {
    dac_ll_update_output_value(DAC_CHAN_0, 128);  // nothing buffered: hold mid-scale
    underruns++;
  }
}

// Stereo 16-bit from A2DP -> mono, level-scaled, unsigned 8-bit into the ring
class RingDacOutput : public AudioStream {
 public:
  volatile uint32_t lastWriteMs = 0;

  size_t write(const uint8_t *data, size_t len) override {
    const int16_t *in = (const int16_t *)data;
    size_t frames = len / 4;
    for (size_t i = 0; i < frames; i++) {
      int32_t mono = ((int32_t)in[2 * i] + in[2 * i + 1]) / 2;
      int32_t sample = 128 + (int32_t)(mono * MAX_LEVEL) / 256;
      size_t next = (ringHead + 1) % RING_SIZE;
      while (next == ringTail) delay(1);  // ring full: wait for the interrupt to drain it
      ring[ringHead] = (uint8_t)constrain(sample, 0, 255);
      ringHead = next;
    }
    lastWriteMs = millis();
    digitalWrite(MUX_INH_PIN, LOW);  // unmute as soon as audio arrives
    return len;
  }
};

RingDacOutput output;
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
  digitalWrite(MUX_INH_PIN, HIGH);  // muted until audio streams

  dacWrite(DAC_PIN, 128);  // powers up DAC channel 1; the interrupt then writes the register directly

  hw_timer_t *timer = timerBegin(TIMER_HZ);
  timerAttachInterrupt(timer, &onSampleTimer);
  timerAlarm(timer, TICKS_PER_SAMPLE, true, 0);

  a2dp_sink.set_on_connection_state_changed(onConnectionState);
  a2dp_sink.set_on_audio_state_changed(onAudioState);
  a2dp_sink.start(BT_NAME);
  Serial.printf("Bluetooth speaker \"%s\" ready, pair from your phone\n", BT_NAME);
}

void loop() {
  static uint32_t lastUnderruns = 0;
  uint32_t u = underruns;
  bool streaming = millis() - output.lastWriteMs < 500;
  if (!streaming) {
    digitalWrite(MUX_INH_PIN, HIGH);  // mute the CD4051B while idle
  } else {
    Serial.printf("streaming, buffer underruns in last 2 s: %u\n", u - lastUnderruns);
  }
  lastUnderruns = u;
  delay(streaming ? 2000 : 100);
}
