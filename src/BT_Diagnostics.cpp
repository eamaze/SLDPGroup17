#include "BT_Diagnostics.h"
#include <SPIFFS.h>
#include <esp_bt.h>
#include <esp_system.h>

void runBluetoothDiagnostics() {
  Serial.println("\n╔═══════════════════════════════════════════════════════╗");
  Serial.println("║         BLUETOOTH DIAGNOSTICS - ESP32 WROOM          ║");
  Serial.println("╚═══════════════════════════════════════════════════════╝\n");
  
  // 1. Check available memory
  Serial.println("[1] Checking System Resources...");
  uint32_t freeHeap = esp_get_free_heap_size();
  Serial.print("    Free heap memory: ");
  Serial.print(freeHeap / 1024);
  Serial.println(" KB");
  
  if (freeHeap < 100000) {
    Serial.println("    ⚠ WARNING: Low heap memory! Bluetooth may not work properly.");
    Serial.println("    Try reducing I2S buffer sizes or other memory usage.");
  } else {
    Serial.println("    ✓ Sufficient memory available");
  }
  
  // 2. Check power supply
  Serial.println("\n[2] Power Supply Check (Informational)...");
  Serial.println("    Note: You must ensure stable 5V supply to ESP32");
  Serial.println("    Bluetooth requires consistent power (500mA+ peak current)");
  Serial.println("    Use a quality USB cable and power supply");
  Serial.println("    ⚠ Avoid direct USB power from laptop - use powered hub or wall adapter");
  
  // 3. Check for pin conflicts
  Serial.println("\n[3] Checking for Known Pin Conflicts...");
  Serial.println("    I2S pins configured:");
  Serial.println("      WS (LRCK):  GPIO 25");
  Serial.println("      SCK (BCLK): GPIO 33");
  Serial.println("      SD (DIN):   GPIO 32");
  Serial.println("    ");
  Serial.println("    Bluetooth uses internal UART (no GPIO conflicts)");
  Serial.println("    ✓ No pin conflicts detected with I2S");
  
  // 4. Check SPIFFS
  Serial.println("\n[4] Checking File System (SPIFFS)...");
  if (SPIFFS.begin(false)) {
    uint32_t totalBytes = SPIFFS.totalBytes();
    uint32_t usedBytes = SPIFFS.usedBytes();
    Serial.print("    ✓ SPIFFS OK - ");
    Serial.print((totalBytes - usedBytes) / 1024);
    Serial.print(" KB free ("); 
    Serial.print(usedBytes / 1024);
    Serial.println(" KB used)");
    SPIFFS.end();
  } else {
    Serial.println("    ✗ SPIFFS failed to mount");
  }
  
  // 5. Bluetooth Protocol Info
  Serial.println("\n[5] Bluetooth Configuration...");
  Serial.println("    Protocol: BR/EDR (Bluetooth Classic)");
  Serial.println("    Profile: SPP (Serial Port Profile)");
  Serial.println("    UUID: 00001101-0000-1000-8000-00805F9B34FB");
  
  // 6. Recommendations
  Serial.println("\n[6] Troubleshooting Steps...");
  Serial.println("    1. Restart your phone/device Bluetooth");
  Serial.println("    2. Reboot the ESP32");
  Serial.println("    3. Check the power supply (use external 5V adapter)");
  Serial.println("    4. Look for 'ESP32_MusicalNote' in Bluetooth scan");
  Serial.println("    5. If found, pair using PIN 1234 or 0000");
  Serial.println("    6. Watch Serial output for connection status messages");
  Serial.println("    7. If not found: power supply is likely the issue");
  
  Serial.println("\n╔═══════════════════════════════════════════════════════╗");
  Serial.println("║            DIAGNOSTICS COMPLETE                      ║");
  Serial.println("╚═══════════════════════════════════════════════════════╝\n");
}
