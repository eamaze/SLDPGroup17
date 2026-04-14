#include <Arduino.h>
#include <driver/i2s.h>
#include "Yin.h"
#include "BluetoothController.h" 

// I2S Pins
#define I2S_WS  25 
#define I2S_SCK 33 
#define I2S_SD  32 
#define I2S_PORT I2S_NUM_0

// --- NEW: TARGET LED SETTINGS ---
#define LED_PIN 2                 // GPIO for the LED (Pin 2 is usually the onboard LED)
const float TARGET_FREQ = 440.0;  // The target frequency in Hz (440.0 = Note A4)
const float TOLERANCE = 50;      // Allow +/- 5 Hz of wiggle room for the note

// --- PITCH RECOGNITION SETTINGS ---
const uint16_t SAMPLES = 1024; 
const uint32_t SAMPLING_FREQUENCY = 44100; 
const int NOISE_THRESHOLD = 300; 

int32_t raw_samples[SAMPLES];
int16_t yin_samples[SAMPLES]; 
Yin yin;

// Bluetooth Controller Instance
BluetoothController btController("ESP32_MusicalNote");

void i2s_install() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLING_FREQUENCY,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, 
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, //AI DO NOT CHANGE, WIRING IS REVERSED
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize the LED pin as an output and ensure it is off
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
// Initialize Bluetooth
  btController.begin();
  
  
  Serial.println("Starting Pitch Detection + LED Trigger...");
  
  Yin_init(&yin, SAMPLES, YIN_DEFAULT_THRESHOLD);
  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);
}

void loop() {
  size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_PORT, &raw_samples, sizeof(raw_samples), &bytesIn, portMAX_DELAY);

  if (result == ESP_OK) {
    int samples_read = bytesIn / 4;
    long total_amplitude = 0;

    for (int i = 0; i < samples_read; i++) {
      int16_t sample = raw_samples[i] >> 14; 
      yin_samples[i] = sample; 
      total_amplitude += abs(sample); 
    }

    int average_volume = total_amplitude / samples_read;

    // Noise Gate: Only process if loud enough
    if (average_volume > NOISE_THRESHOLD) {
      
      float pitch = Yin_getPitch(&yin, yin_samples);
      
      if (pitch != -1.0 && pitch > 80.0 && pitch < 2000.0) {
        
        Serial.print("Pitch: ");
        Serial.print(pitch);
        Serial.println(" Hz");
bool isHit = abs(pitch - TARGET_FREQ) <= TOLERANCE;
        if (isHit) {
          digitalWrite(LED_PIN, HIGH); // You hit the note! Turn on LED.
        } else {
          digitalWrite(LED_PIN, LOW);  // Wrong note. Turn off LED.
        }
        
        // --- NEW: SEND DATA OVER BLUETOOTH ---
        btController.sendPitchData(pitch, TARGET_FREQ, isHit);

      } else {
        // If it's just noisy garbage data and YIN can't find a pitch, ensure LED is off
        digitalWrite(LED_PIN, LOW);
      }
    } else {
      // If the room is quiet, ensure the LED is off
      digitalWrite(LED_PIN, LOW);
    }
    
    // --- NEW: HANDLE INCOMING BLUETOOTH COMMANDS ---
    btController.handleIncomingData(); else {
      // If the room is quiet, ensure the LED is off
      digitalWrite(LED_PIN, LOW);
    }
  }
}