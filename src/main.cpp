#include <Arduino.h>

// ESP8266 build: no ESP32 I2S driver
#include "Yin.h"
#include "BluetoothController.h"
#include "LedController.h"
#include "SongManager.h"

// --- INMP441 pin mapping (ESP8266) ---
// Note: Actual I2S RX on ESP8266 is limited; this project provides a stub for sampling
// and focuses on WiFi WebSocket + note/game logic.
#define INMP441_BCLK 14 // D5
#define INMP441_WS   15 // D8
#define INMP441_DOUT 13 // D7

// --- LED mapping (2 LEDs only) ---
#define RIGHT_LED 4 // D2
#define WRONG_LED 5 // D1

// Dummy mapping to satisfy existing LedController which expects 12 pins.
// Only index 0 is used by this simplified build; others are set to 255 and ignored.
static const uint8_t TARGET_LEDS[12] = {255,255,255,255,255,255,255,255,255,255,255,255};

// --- PITCH SETTINGS tuned down for ESP8266 ---
static const uint16_t SAMPLES = 256;
static const uint32_t SAMPLING_FREQUENCY = 16000;
static const int NOISE_THRESHOLD = 600;
static const float FREQUENCY_TOLERANCE = 25.0f;

static int16_t yin_samples[SAMPLES];
static Yin yin;

BluetoothController btController("Keystroke-ESP8266");
LedController ledController(TARGET_LEDS, RIGHT_LED, WRONG_LED);
SongManager songManager(&ledController);

// TODO: Replace with real INMP441 sampling implementation for ESP8266
static bool readMicSamples(int16_t* out, size_t count) {
  (void)out;
  (void)count;
  return false;
}

// Convert frequency to MIDI note number (rough)
static int freqToMidi(float f) {
  if (f <= 0) return -1;
  float n = 69.0f + 12.0f * logf(f / 440.0f) / logf(2.0f);
  return (int)lroundf(n);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  ledController.begin();
  songManager.setLeniencyWindow(150);
  songManager.setAudioLatencyOffset(80);
  songManager.setFrequencyTolerance(FREQUENCY_TOLERANCE);

  // AP mode + WebSocket server
  btController.setApCredentials("Keystroke-ESP8266", "keystroke123");
  btController.begin();

  // YIN init
  Yin_init(&yin, SAMPLES, YIN_DEFAULT_THRESHOLD);

  Serial.println("=== READY (ESP8266 AP + WebSocket) ===");
  Serial.println("Connect phone to WiFi SSID: Keystroke-ESP8266");
  Serial.println("Then connect WS: ws://192.168.4.1:81/");
}

void loop() {
  btController.handleIncomingData();
  ledController.update();

  // Handle start
  if (btController.checkStartCommand() && songManager.getState() == SONG_LOADED) {
    songManager.startPlaying();
    btController.sendData("Playback Started!");
  }

  // Handle new file
  String newFile = btController.checkNewFileTransfer();
  if (newFile.length() > 0) {
    uint16_t bpm = btController.getBPM();
    songManager.loadSong(newFile.c_str(), bpm);
    btController.sendData("Loaded! Awaiting BEGINSONG.");
  }

  // Pitch detect + send note updates
  float detectedPitch = -1.0f;
  if (readMicSamples(yin_samples, SAMPLES)) {
    long total = 0;
    for (size_t i = 0; i < SAMPLES; i++) total += abs(yin_samples[i]);
    int avg = (int)(total / (long)SAMPLES);

    if (avg > NOISE_THRESHOLD) {
      float pitch = Yin_getPitch(&yin, yin_samples);
      if (pitch > 80.0f && pitch < 2000.0f) detectedPitch = pitch;
    }
  }

  static uint32_t lastSend = 0;
  static int lastMidi = -999;
  if (millis() - lastSend >= 150) {
    int midi = freqToMidi(detectedPitch);
    if (midi != lastMidi) {
      lastMidi = midi;
      if (midi >= 0) {
        btController.sendData("NOTE:" + String(midi));
      }
    }
    lastSend = millis();
  }

  // Update playhead with detected pitch if in game mode
  if (songManager.getState() == SONG_PLAYING) {
    songManager.updatePlayhead(detectedPitch);
  }

  if (songManager.getState() == SONG_FINISHED) {
    float accuracy = songManager.getAccuracy();
    btController.sendData("Song Complete! Accuracy: " + String(accuracy, 1) + "%");
    btController.sendSongCompleted();
    songManager.resetSong();
  }
}
