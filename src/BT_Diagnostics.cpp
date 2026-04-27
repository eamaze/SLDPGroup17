#include "BT_Diagnostics.h"

#include <LittleFS.h>
#include <ESP8266WiFi.h>

void runBluetoothDiagnostics() {
  Serial.println("\n=== ESP8266 NETWORK/FS DIAGNOSTICS ===\n");

  Serial.print("[Heap] Free: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");

  Serial.print("[WiFi] Mode: ");
  Serial.println((int)WiFi.getMode());

  Serial.print("[WiFi] AP IP: ");
  Serial.println(WiFi.softAPIP());

  FSInfo info;
  if (LittleFS.info(info)) {
    Serial.print("[LittleFS] Total: ");
    Serial.print(info.totalBytes);
    Serial.print(" Used: ");
    Serial.print(info.usedBytes);
    Serial.print(" Free: ");
    Serial.println(info.totalBytes - info.usedBytes);
  } else {
    Serial.println("[LittleFS] info() failed (is it mounted?)");
  }

  Serial.println("\n=== END DIAGNOSTICS ===\n");
}
