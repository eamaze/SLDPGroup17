#include <Arduino.h>
#include <driver/i2s.h>
#include "Yin.h"
#include "BluetoothController.h"
#include "LedController.h"
#include "SongManager.h"
#include "BT_Diagnostics.h"

// I2S Pins
#define I2S_WS 25
#define I2S_SCK 33
#define I2S_SD 32
#define I2S_PORT I2S_NUM_0

// --- HARDWARE MAPPING SETTINGS ---
// Note: Some of these pins might conflict or be input only depending on your exact board.
// Replace with the physical output pins connected to your LEDs or Shift Registers.
const uint8_t WHITE_LEDS[12] = {2, 4, 5, 12, 13, 14, 15, 18, 19, 21, 22, 23}; 
const uint8_t RED_LEDS[12]   = {26, 27, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Fill in remaining pins!

// --- PITCH RECOGNITION SETTINGS ---
const uint16_t SAMPLES = 1024;
const uint32_t SAMPLING_FREQUENCY = 44100;
const int NOISE_THRESHOLD = 2000;
const float FREQUENCY_TOLERANCE = 25.0;

int32_t raw_samples[SAMPLES];
int16_t yin_samples[SAMPLES];
Yin yin;

// Controllers and managers
BluetoothController btController("ESP32_MusicalNote");
LedController ledController(WHITE_LEDS, RED_LEDS);
SongManager songManager(&ledController);

void i2s_install() {
  const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLING_FREQUENCY,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, 
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 512,
      .use_apll = false};
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD};
  i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  delay(2000);  

  ledController.begin();
  
  // Set our new configurable timing tolerances
  songManager.setLeniencyWindow(150);      // +/- 150ms hit box
  songManager.setAudioLatencyOffset(80);   // Account for Yin calculation time
  songManager.setFrequencyTolerance(FREQUENCY_TOLERANCE);

  btController.begin();
  delay(2000);
  
  Yin_init(&yin, SAMPLES, YIN_DEFAULT_THRESHOLD);
  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);
  
  Serial.println("\n=== READY. AWAITING BPM, FILE, AND START COMMAND ===");
}

void loop() {
  btController.handleIncomingData();
  ledController.update(); // Maintain red LED pulse timers

  // --- Process Bluetooth Commands ---
  // 1. Check if the "START" command was triggered via Bluetooth
  if (btController.checkStartCommand() && songManager.getState() == SONG_LOADED) {
    songManager.startPlaying();
    btController.sendData("Playback Started!");
  }

  // 2. Check if a new MIDI file just finished transferring
  String newFile = btController.checkNewFileTransfer();
  if (newFile.length() > 0) {
    Serial.println("\n*** NEW SONG RECEIVED ***");
    
    // Grab the latest BPM from the Bluetooth controller
    uint16_t currentBpm = btController.getBPM();
    
    // Load the newly received file with the provided BPM
    songManager.loadSong(newFile.c_str(), currentBpm);
    btController.sendData("Loaded! Awaiting START command.");
  }


  // --- Process Audio and Playhead ---
  float detectedPitch = -1.0;
  
  // Only process audio if we are actively playing
  if (songManager.getState() == SONG_PLAYING) {
    size_t bytesIn = 0;
    esp_err_t result = i2s_read(I2S_PORT, &raw_samples, sizeof(raw_samples), &bytesIn, 0); // Non-blocking read

    if (result == ESP_OK && bytesIn > 0) {
      int samples_read = bytesIn / 4;
      long total_amplitude = 0;

      for (int i = 0; i < samples_read; i++) {
        int16_t sample = raw_samples[i] >> 14;
        yin_samples[i] = sample;
        total_amplitude += abs(sample);
      }

      int average_volume = total_amplitude / samples_read;
      if (average_volume > NOISE_THRESHOLD) {
        float pitch = Yin_getPitch(&yin, yin_samples);
        if (pitch > 80.0 && pitch < 2000.0) {
          detectedPitch = pitch;
        }
      }
    }
    
    // Update playhead with the pitch (or -1.0 if silent)
    songManager.updatePlayhead(detectedPitch);
  }
  
  
  // --- Handle Song Completion ---
  // Check if song just finished
  if (songManager.getState() == SONG_FINISHED) {
    float accuracy = songManager.getAccuracy();
    String accMsg = "Song Complete! Accuracy: " + String(accuracy, 1) + "%";
    btController.sendData(accMsg);
    btController.sendSongCompleted();
    
    // Reset to idle/loaded to await restart
    songManager.resetSong();
  }
}