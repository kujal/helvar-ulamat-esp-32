#include <Arduino.h>
#include <ESP_I2S.h>
#include <driver/gpio.h>
#include <math.h>

// MAX98357A wiring
const int I2S_BCLK = 33;  // -> BCLK
const int I2S_LRC = 32;   // -> LRC
const int I2S_DOUT = 25;  // -> DIN

const int SAMPLE_RATE = 44100;
const float VOLUME = 0.5f;

I2SClass i2s;

// Samples the I2S pins from core 0 to confirm they are actually toggling
void pinMonitorTask(void *) {
  const int pins[] = {I2S_BCLK, I2S_LRC, I2S_DOUT};
  const char *names[] = {"BCLK", "LRC", "DIN"};
  for (int i = 0; i < 3; i++) gpio_input_enable((gpio_num_t)pins[i]);
  while (true) {
    for (int i = 0; i < 3; i++) {
      int highs = 0, edges = 0, last = gpio_get_level((gpio_num_t)pins[i]);
      for (int n = 0; n < 20000; n++) {
        int v = gpio_get_level((gpio_num_t)pins[i]);
        highs += v;
        edges += (v != last);
        last = v;
      }
      Serial.printf("%s(GPIO%d): high %3d%%, edges %5d   ", names[i], pins[i], highs / 200, edges);
    }
    Serial.println();
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("I2S init failed");
    while (true) delay(1000);
  }
  Serial.println("Continuous 440 Hz tone (32-bit)");
  xTaskCreatePinnedToCore(pinMonitorTask, "pinmon", 4096, nullptr, 1, nullptr, 0);
}

void loop() {
  const int FRAMES = 256;
  static int32_t buf[FRAMES * 2];
  static float phase = 0;
  const float phaseStep = 2 * PI * 440 / SAMPLE_RATE;
  for (int i = 0; i < FRAMES; i++) {
    int32_t s = (int32_t)(2147483647.0f * VOLUME * sinf(phase));
    buf[2 * i] = s;
    buf[2 * i + 1] = s;
    phase += phaseStep;
    if (phase >= 2 * PI) phase -= 2 * PI;
  }
  i2s.write((uint8_t *)buf, sizeof(buf));
}
