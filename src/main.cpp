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

// --- LED SETTINGS ---
#define LED_PIN 2                // GPIO for the LED

// --- PITCH RECOGNITION SETTINGS ---
const uint16_t SAMPLES = 1024;
const uint32_t SAMPLING_FREQUENCY = 44100;
const int NOISE_THRESHOLD = 300;
const float FREQUENCY_TOLERANCE = 50;  // Allow +/- 50 Hz variation

int32_t raw_samples[SAMPLES];
int16_t yin_samples[SAMPLES];
Yin yin;

// Controllers and managers
BluetoothController btController("ESP32_MusicalNote");
LedController ledController(LED_PIN);
SongManager songManager;

// State tracking
unsigned long lastPitchPrintTime = 0;
const unsigned long PITCH_PRINT_INTERVAL = 500;  // Print pitch every 500ms

// Song initialization state
bool songInitialized = false;

// Bluetooth status tracking
unsigned long lastBTStatusCheck = 0;
const unsigned long BT_STATUS_CHECK_INTERVAL = 5000;  // Check every 5 seconds
bool lastBTConnected = false;

void i2s_install()
{
  const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLING_FREQUENCY,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // AI DO NOT CHANGE, WIRING IS REVERSED
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 512,
      .use_apll = false};

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin()
{
  const i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD};

  i2s_set_pin(I2S_PORT, &pin_config);
}

void initializeSong()
{
  Serial.println("\n=== SONG INITIALIZATION ===");
  
  // Try to load existing song
  String lastFile = btController.getLastMIDFile();
  if (lastFile.length() > 0) {
    String fullPath = String("/midi/") + lastFile;
    Serial.print("Found existing song: ");
    Serial.println(fullPath);
    
    if (songManager.loadSong(fullPath.c_str())) {
      Serial.println("✓ Song loaded successfully!");
      songInitialized = true;
      return;
    }
  }
  
  // No existing song, wait for one to be transferred
  Serial.println("No song file found. Waiting for MIDI file transfer via Bluetooth...");
  Serial.println("Use the Bluetooth connection to send a .MID file.");
  songInitialized = false;
}

void monitorBluetoothStatus() {
  unsigned long currentTime = millis();
  if (currentTime - lastBTStatusCheck >= BT_STATUS_CHECK_INTERVAL) {
    lastBTStatusCheck = currentTime;
    
    bool btConnected = btController.isConnectedToBT();
    
    if (btConnected != lastBTConnected) {
      lastBTConnected = btConnected;
      if (btConnected) {
        Serial.println("\n[BT] ✓ CLIENT CONNECTED");
        btController.sendData("Connected to ESP32!");
      } else {
        Serial.println("\n[BT] ✗ Client disconnected - waiting for new connection");
      }
    } else if (!songManager.isSongLoaded()) {
      // Periodically remind user to send a song if not loaded yet
      if (!btConnected) {
        Serial.println("[BT] Waiting for: 1) Bluetooth connection, 2) MIDI file transfer");
      }
    }
  }
}

void handleNewSongReceived()
{
  String newFile = btController.checkNewFileTransfer();
  if (newFile.length() > 0) {
    Serial.println("\n*** NEW SONG RECEIVED ***");
    Serial.print("File: ");
    Serial.println(newFile);
    
    // Signal song manager about new song
    songManager.newSongReceived(newFile.c_str());
    
    // Send notification to Bluetooth client
    btController.sendData("New song loaded: " + newFile);
    btController.sendData("Ready to play!");
    
    // Sound an alert (could be multiple LED flashes)
    for (int i = 0; i < 3; i++) {
      ledController.turnOn();
      delay(100);
      ledController.turnOff();
      delay(100);
    }
  }
}

void printSongStatus()
{
  if (songManager.isSongLoaded()) {
    Serial.print("[SONG STATUS] Note ");
    Serial.print(songManager.getCurrentNoteIndex() + 1);
    Serial.print(" of ");
    Serial.print(songManager.getTotalNotes());
    Serial.print(" | Target: MIDI ");
    Serial.print(songManager.getCurrentTargetNote());
    Serial.print(" (");
    Serial.print(songManager.getCurrentTargetFrequency(), 1);
    Serial.println(" Hz)");
  }
}

void setup()
{
  Serial.begin(115200);
  delay(2000);  // Give USB serial time to enumerate - increased from 1000ms
  Serial.flush();  // Ensure all buffered data is sent

  Serial.println("\n\n=== SLDP - Song-based LED Detection Program ===");
  
  // Run Bluetooth diagnostics BEFORE initializing Bluetooth
  // This helps identify issues early
  runBluetoothDiagnostics();
  
  // Initialize LED controller
  ledController.begin();
  Serial.println("✓ LED initialized on GPIO " + String(LED_PIN));
  
  // Initialize Bluetooth with extra delay for stability
  Serial.println("\n[SETUP] Initializing Bluetooth...");
  btController.begin();
  delay(2000);  // Give Bluetooth time to become discoverable
  
  // Initialize pitch detection
  Yin_init(&yin, SAMPLES, YIN_DEFAULT_THRESHOLD);
  Serial.println("✓ Yin pitch detection initialized");
  
  // Initialize I2S audio input
  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);
  Serial.println("✓ I2S audio input started");
  
  // Initialize song
  initializeSong();
  
  Serial.println("\n=== WAITING FOR INPUT ===");
}

void loop()
{
  // Monitor Bluetooth connection status periodically
  monitorBluetoothStatus();
  
  // Check for new song transfers
  handleNewSongReceived();
  
  // If no song is loaded, wait for one
  if (!songManager.isSongLoaded()) {
    btController.handleIncomingData();
    delay(100);
    return;
  }
  
  // Read audio samples from I2S
  size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_PORT, &raw_samples, sizeof(raw_samples), &bytesIn, portMAX_DELAY);

  if (result == ESP_OK)
  {
    int samples_read = bytesIn / 4;
    long total_amplitude = 0;

    // Convert samples and calculate volume
    for (int i = 0; i < samples_read; i++)
    {
      int16_t sample = raw_samples[i] >> 14;
      yin_samples[i] = sample;
      total_amplitude += abs(sample);
    }

    int average_volume = total_amplitude / samples_read;

    // Noise Gate: Only process if loud enough
    if (average_volume > NOISE_THRESHOLD)
    {
      float pitch = Yin_getPitch(&yin, yin_samples);

      if (pitch != -1.0 && pitch > 80.0 && pitch < 2000.0)
      {
        // Print pitch periodically for debugging
        unsigned long currentTime = millis();
        if (currentTime - lastPitchPrintTime >= PITCH_PRINT_INTERVAL) {
          Serial.print("[PITCH] ");
          Serial.print(pitch, 1);
          Serial.print(" Hz | Target: ");
          Serial.print(songManager.getCurrentTargetFrequency(), 1);
          Serial.println(" Hz");
          lastPitchPrintTime = currentTime;
        }

        float targetFreq = songManager.getCurrentTargetFrequency();
        bool isHit = abs(pitch - targetFreq) <= FREQUENCY_TOLERANCE;
        
        // Check if note was hit
        if (isHit && !songManager.shouldShowConfirmation())
        {
          // Note hit! Move to next note
          Serial.println("-----> NOTE HIT! <-----");
          bool hasMore = songManager.noteHit();
          
          if (!hasMore) {
            // Song finished!
            Serial.println("\n╔════════════════════════════╗");
            Serial.println("║   SONG COMPLETED! ✓✓✓     ║");
            Serial.println("╚════════════════════════════╝\n");
            
            // Flash LED multiple times to celebrate
            for (int i = 0; i < 5; i++) {
              ledController.turnOn();
              delay(100);
              ledController.turnOff();
              delay(100);
            }
            
            // Wait a moment then restart
            delay(1000);
            Serial.println("Restarting song...\n");
            songManager.restartSong();
          } else {
            // Print next note information
            printSongStatus();
          }
        }
        
        // Send pitch data to Bluetooth
        btController.sendPitchData(pitch, targetFreq, isHit);
      }
      else
      {
        // No valid pitch detected, ensure LED is off
        ledController.turnOff();
      }
    }
    else
    {
      // Too quiet, ensure LED is off
      ledController.turnOff();
    }
  }
  
  // Handle LED confirmation flash
  if (songManager.shouldShowConfirmation()) {
    ledController.turnOn();
  } else {
    ledController.turnOff();
  }
  
  // Handle incoming Bluetooth commands
  btController.handleIncomingData();
}