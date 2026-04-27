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

// --- GPIO MAPPING ---
// 12 Target Notes
const uint8_t TARGET_LEDS[12] = {2, 4, 5, 12, 13, 14, 15, 18, 19, 21, 22, 23}; 
// Status LEDs
const uint8_t CORRECT_LED = 26; 
const uint8_t MISS_LED    = 27; 

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
LedController ledController(TARGET_LEDS, CORRECT_LED, MISS_LED);
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
  
  songManager.setLeniencyWindow(150);      
  songManager.setAudioLatencyOffset(80);   
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
  ledController.update(); 

  // --- Process Bluetooth Commands ---
  if (btController.checkStartCommand() && songManager.getState() == SONG_LOADED) {
    songManager.startPlaying();
    btController.sendData("Playback Started!");
  }

  String newFile = btController.checkNewFileTransfer();
  if (newFile.length() > 0) {
    Serial.println("\n*** NEW SONG RECEIVED ***");
    uint16_t currentBpm = btController.getBPM();
    songManager.loadSong(newFile.c_str(), currentBpm);
    btController.sendData("Loaded! Awaiting START command.");
  }

  // --- Process Audio and Playhead ---
  float detectedPitch = -1.0;
  
  if (songManager.getState() == SONG_PLAYING) {
    size_t bytesIn = 0;
    esp_err_t result = i2s_read(I2S_PORT, &raw_samples, sizeof(raw_samples), &bytesIn, 0);

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
    
    songManager.updatePlayhead(detectedPitch);
  }
  
  // --- Handle Song Completion ---
  if (songManager.getState() == SONG_FINISHED) {
    float accuracy = songManager.getAccuracy();
    String accMsg = "Song Complete! Accuracy: " + String(accuracy, 1) + "%";
    btController.sendData(accMsg);
    btController.sendSongCompleted();
    
    songManager.resetSong(); 
  }
}