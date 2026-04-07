/**
 * ESP32 Pitch Detector with LED Feedback
 * =======================================
 * Uses I2S microphone (INMP441 or similar) + arduinoFFT for pitch detection.
 * When a detected note matches a preset target note, an LED lights up.
 *
 * Wiring:
 *   INMP441 Microphone:
 *     VDD  -> 3.3V
 *     GND  -> GND
 *     SD   -> GPIO 32  (I2S_DATA_IN)
 *     SCK  -> GPIO 14  (I2S_BCLK)
 *     WS   -> GPIO 15  (I2S_LRCLK)
 *     L/R  -> GND      (selects LEFT channel)
 *
 *   LED:
 *     Anode (+) -> 330Ω resistor -> GPIO 2
 *     Cathode(-) -> GND
 */

#include <Arduino.h>
#include <driver/i2s.h>
#include "arduinoFFT.h"
#include "pitch_detector.h"
#include "note_matcher.h"

// ─── Pin Configuration ────────────────────────────────────────────────────────
#define I2S_WS_PIN    15   // Word Select (LRCLK)
#define I2S_BCLK_PIN  14   // Bit Clock
#define I2S_DATA_PIN  32   // Data In (SD)
#define LED_PIN        2   // Onboard LED or external LED

// ─── Audio Configuration ──────────────────────────────────────────────────────
#define SAMPLE_RATE        44100   // Hz
#define FFT_SIZE           2048    // Must be power of 2; higher = better freq resolution
#define I2S_PORT           I2S_NUM_0
#define AMPLITUDE_THRESHOLD 500.0  // Ignore noise below this magnitude

// ─── FFT Buffers ─────────────────────────────────────────────────────────────
double vReal[FFT_SIZE];
double vImag[FFT_SIZE];

ArduinoFFT<double> FFT(vReal, vImag, FFT_SIZE, SAMPLE_RATE);

// ─── Objects ──────────────────────────────────────────────────────────────────
PitchDetector pitchDetector(SAMPLE_RATE, FFT_SIZE, AMPLITUDE_THRESHOLD);
NoteMatcher   noteMatcher;

// ─── I2S Setup ────────────────────────────────────────────────────────────────
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 8,
    .dma_buf_len          = 512,
    .use_apll             = true,   // Use APLL for accurate sample rate
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_BCLK_PIN,
    .ws_io_num    = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_DATA_PIN
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_start(I2S_PORT);

  Serial.println("[I2S] Initialized OK");
}

// ─── Read samples from I2S into FFT buffer ────────────────────────────────────
bool readAudioSamples() {
  int32_t raw[FFT_SIZE];
  size_t bytesRead = 0;

  esp_err_t result = i2s_read(
    I2S_PORT,
    raw,
    sizeof(raw),
    &bytesRead,
    portMAX_DELAY
  );

  if (result != ESP_OK || bytesRead < sizeof(raw)) {
    Serial.println("[I2S] Read error or incomplete buffer");
    return false;
  }

  // Normalise 32-bit I2S samples (top 24 bits are audio, bottom 8 are padding)
  for (int i = 0; i < FFT_SIZE; i++) {
    vReal[i] = (double)(raw[i] >> 8) / 8388608.0;  // scale to -1.0 … +1.0
    vImag[i] = 0.0;
  }
  return true;
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 Pitch Detector ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  setupI2S();

  // Configure the target note(s) to match — change these to suit your needs!
  // Available notes: C, C#, D, D#, E, F, F#, G, G#, A, A#, B
  // Octave range: 0–8
  noteMatcher.addTargetNote("A", 4);   // A4 = 440 Hz (concert pitch A)
  noteMatcher.addTargetNote("E", 4);   // E4 ≈ 329.63 Hz
  // noteMatcher.addTargetNote("C", 4);  // Middle C ≈ 261.63 Hz — uncomment to add more

  noteMatcher.printTargets();

  Serial.println("[READY] Listening for notes...\n");
}

// ─── Main Loop ────────────────────────────────────────────────────────────────
void loop() {
  if (!readAudioSamples()) return;

  // Apply Hann window to reduce spectral leakage
  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  // Detect dominant frequency
  double dominantFreq = pitchDetector.detectPitch(vReal);

  if (dominantFreq > 0.0) {
    String noteName   = "";
    int    noteOctave = 0;
    double deviation  = 0.0;

    pitchDetector.freqToNote(dominantFreq, noteName, noteOctave, deviation);

    Serial.printf("[PITCH] %.2f Hz  →  %s%d  (%.1f cents off)\n",
                  dominantFreq, noteName.c_str(), noteOctave, deviation);

    // Check if detected note matches any preset target
    bool matched = noteMatcher.isMatch(noteName, noteOctave);
    digitalWrite(LED_PIN, matched ? HIGH : LOW);

    if (matched) {
      Serial.printf("  ★ MATCH! LED ON  (%s%d)\n", noteName.c_str(), noteOctave);
    }
  } else {
    // No significant pitch detected — silence / noise
    digitalWrite(LED_PIN, LOW);
  }

  // Small delay to avoid flooding serial; remove for fastest response
  delay(50);
}
