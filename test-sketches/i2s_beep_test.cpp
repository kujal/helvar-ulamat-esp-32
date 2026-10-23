#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>

// MAX98357A wiring
const int I2S_BCLK = 33;  // -> BCLK
const int I2S_LRC = 32;   // -> LRC
const int I2S_DOUT = 25;  // -> DIN
const int LED_PIN = 2;    // on-board LED, if the board has one here

const int SAMPLE_RATE = 44100;
// Peak level as a fraction of full scale; keep low for small speakers
const float VOLUME = 0.25f;

I2SClass i2s;

void playTone(float freq, uint32_t durationMs) {
  const int FRAMES = 256;
  int16_t buf[FRAMES * 2];  // stereo: same sample in L and R
  static float phase = 0;
  const float phaseStep = 2 * PI * freq / SAMPLE_RATE;
  uint32_t totalFrames = (uint64_t)SAMPLE_RATE * durationMs / 1000;

  for (uint32_t done = 0; done < totalFrames; done += FRAMES) {
    for (int i = 0; i < FRAMES; i++) {
      int16_t s = (int16_t)(32767 * VOLUME * sinf(phase));
      buf[2 * i] = s;
      buf[2 * i + 1] = s;
      phase += phaseStep;
      if (phase >= 2 * PI) phase -= 2 * PI;
    }
    i2s.write((uint8_t *)buf, sizeof(buf));
  }
}

void silence(uint32_t durationMs) {
  const int FRAMES = 256;
  int16_t buf[FRAMES * 2] = {0};
  uint32_t totalFrames = (uint64_t)SAMPLE_RATE * durationMs / 1000;
  for (uint32_t done = 0; done < totalFrames; done += FRAMES) {
    i2s.write((uint8_t *)buf, sizeof(buf));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("I2S init failed");
    while (true) delay(1000);
  }
  Serial.println("MAX98357A test: 440 Hz / 880 Hz beeps");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("440 Hz");
  playTone(440, 700);
  digitalWrite(LED_PIN, LOW);
  silence(300);

  digitalWrite(LED_PIN, HIGH);
  Serial.println("880 Hz");
  playTone(880, 700);
  digitalWrite(LED_PIN, LOW);
  silence(1000);
}
