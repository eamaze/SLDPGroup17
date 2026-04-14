#include "BluetoothController.h"

// --- BLE CALLBACK CLASSES ---

// Handles device connection and disconnection
class MyServerCallbacks: public BLEServerCallbacks {
    BluetoothController* controller;
public:
    MyServerCallbacks(BluetoothController* c) : controller(c) {}
    void onConnect(BLEServer* pServer) {
      controller->setConnectionState(true);
    };
    void onDisconnect(BLEServer* pServer) {
      controller->setConnectionState(false);
    }
};

// Handles incoming data from the connected device
class MyCallbacks: public BLECharacteristicCallbacks {
    BluetoothController* controller;
public:
    MyCallbacks(BluetoothController* c) : controller(c) {}
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        controller->processReceivedData((uint8_t*)rxValue.data(), rxValue.length());
      }
    }
};

// --- CONTROLLER IMPLEMENTATION ---

BluetoothController::BluetoothController(const char* name) 
  : deviceName(name), ftState(FT_IDLE), 
    bytesReceived(0), expectedFileSize(0), fileChecksum(0), 
    newFileTransferFlag(false), pServer(NULL), pTxCharacteristic(NULL),
    deviceConnected(false), oldDeviceConnected(false), commandQueue("") {
}

void BluetoothController::begin() {
  // Initialize SPIFFS for file storage
  initSPIFFS();
  
  Serial.println("\n[BT] ========== BLE INITIALIZATION ==========");
  Serial.print("[BT] Device Name: ");
  Serial.println(deviceName);
  
  // Create the BLE Device
  BLEDevice::init(deviceName);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks(this));

  // Create the BLE Service (Nordic UART Service)
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic for TX (Sending to app)
  pTxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pTxCharacteristic->addDescriptor(new BLE2902());

  // Create a BLE Characteristic for RX (Receiving from app)
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                       BLECharacteristic::PROPERTY_WRITE | 
                       BLECharacteristic::PROPERTY_WRITE_NR
                     );
  pRxCharacteristic->setCallbacks(new MyCallbacks(this));

  // Start the service
  pService->start();

  // Configure advertising for iOS/Android compatibility
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  
  // Enable scan response for better discoverability
  pAdvertising->setScanResponse(true);
  
  // Set appearance for proper device classification
  pAdvertising->setAppearance(ESP_BLE_APPEARANCE_GENERIC_PHONE);
  
  // Set preferred connection parameters
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  
  // Start advertising
  pAdvertising->start();
  
  Serial.println("[BT] ✓ BLE Initialized and Advertising!");
  Serial.println("[BT] Device name: " + String(deviceName));
  Serial.println("[BT] Look for device in your phone's Bluetooth settings");
  Serial.println("[BT] ==================================================\n");
}

void BluetoothController::initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("[FS] Failed to mount SPIFFS");
    return;
  }
  
  Serial.println("[FS] SPIFFS mounted successfully");
  
  // Create MIDI directory if it doesn't exist
  if (!SPIFFS.exists(MID_FILE_DIR)) {
    SPIFFS.mkdir(MID_FILE_DIR);
    Serial.println("[FS] Created /midi/ directory");
  }
  
  uint32_t totalBytes = SPIFFS.totalBytes();
  uint32_t usedBytes = SPIFFS.usedBytes();
  Serial.print("[FS] Total: ");
  Serial.print(totalBytes);
  Serial.print(" bytes, Used: ");
  Serial.print(usedBytes);
  Serial.println(" bytes");
}

void BluetoothController::setConnectionState(bool state) {
    deviceConnected = state;
}

bool BluetoothController::isConnectedToBT() {
  return deviceConnected;
}

void BluetoothController::sendData(const String& data) {
  if (deviceConnected && pTxCharacteristic != NULL) {
    String message = data + "\n";
    pTxCharacteristic->setValue(message.c_str());
    pTxCharacteristic->notify();
  }
}

void BluetoothController::sendPitchData(float pitch, float targetFreq, bool isHit) {
  if (deviceConnected && pTxCharacteristic != NULL) {
    String message = String(pitch, 2) + "Hz";
    if (isHit) {
      message += " [HIT!]";
    }
    message += "\n";
    pTxCharacteristic->setValue(message.c_str());
    pTxCharacteristic->notify();
  }
}

void BluetoothController::processReceivedData(uint8_t* data, size_t length) {
  if (ftState == FT_RECEIVING) {
    // If receiving a file, treat incoming data as raw binary bytes
    if (!receiveFileData(data, length)) {
      cancelFileTransfer();
      sendData("ERROR:WRITE_FAILED");
    }
  } else {
    // If NOT receiving a file, treat data as characters for text commands
    for (size_t i = 0; i < length; i++) {
      commandQueue += (char)data[i];
    }
  }
}

void BluetoothController::handleIncomingData() {
  // Handle Disconnection/Reconnection advertising
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); // Give stack chance to get things ready
      pServer->startAdvertising(); // Restart advertising
      Serial.println("[BT] Restarting BLE Advertising...");
      oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }

  // Process any text commands sitting in the queue
  int newlineIdx = commandQueue.indexOf('\n');
  if (newlineIdx == -1) newlineIdx = commandQueue.indexOf('\r'); // Catch carriage returns too

  while (newlineIdx >= 0) {
    // Extract the command
    String command = commandQueue.substring(0, newlineIdx);
    
    // Remove the command from the queue
    commandQueue = commandQueue.substring(newlineIdx + 1);
    command.trim(); 
    
    if (command.length() > 0) {
      Serial.print("[BT] Received: ");
      Serial.println(command);
      
      // Process specific commands
      if (command.startsWith("FILE_START:") || 
          command.startsWith("FILE_DATA:") || 
          command.startsWith("FILE_END:")) {
        handleFileTransferCommand(command);
      } else if (command == "HELP") {
        sendData("=== Available Commands ===");
        sendData("STATUS - Get current status");
        sendData("INFO - Get device info");
        sendData("LIST_MIDI - List stored MIDI files");
        sendData("FILE_START:filename.mid:filesize");
        sendData("FILE_END:checksum");
        sendData("DELETE:filename.mid");
      } else if (command == "STATUS") {
        sendData("Device is running");
        String ftStatus = (ftState == FT_IDLE) ? "Idle" : (ftState == FT_RECEIVING) ? "Receiving file" : "Unknown";
        sendData("File transfer status: " + ftStatus);
      } else if (command == "INFO") {
        sendData("ESP32 Musical Note Detector BLE v3.0");
      } else if (command == "LIST_MIDI") {
        listMIDFiles();
      } else if (command.startsWith("DELETE:")) {
        String filename = command.substring(7);
        if (deleteMIDFile(filename.c_str())) {
          sendData("File deleted: " + filename);
        } else {
          sendData("Failed to delete: " + filename);
        }
      }
    }
    
    // Check if there are more commands in the remaining queue
    newlineIdx = commandQueue.indexOf('\n');
    if (newlineIdx == -1) newlineIdx = commandQueue.indexOf('\r');
  }
}

void BluetoothController::handleFileTransferCommand(const String& command) {
  if (command.startsWith("FILE_START:")) {
    // Parse: FILE_START:filename.mid:size
    int secondColon = command.indexOf(':', 11);
    if (secondColon > 0) {
      String filename = command.substring(11, secondColon);
      String sizeStr = command.substring(secondColon + 1);
      uint32_t fileSize = sizeStr.toInt();
      
      if (startFileTransfer(filename, fileSize)) {
        sendData("OK:FILE_TRANSFER_STARTED:" + filename);
      } else {
        sendData("ERROR:FILE_TRANSFER_FAILED");
      }
    }
  } 
  else if (command.startsWith("FILE_END:")) {
    // Parse: FILE_END:checksum
    String checksumStr = command.substring(9);
    uint32_t checksum = (uint32_t)checksumStr.toInt();
    
    if (endFileTransfer(checksum)) {
      sendData("OK:FILE_TRANSFER_COMPLETE");
    } else {
      sendData("ERROR:CHECKSUM_MISMATCH");
      cancelFileTransfer();
    }
  }
}

bool BluetoothController::startFileTransfer(const String& filename, uint32_t fileSize) {
  if (filename.length() == 0 || filename.length() > MAX_FILENAME_LENGTH) {
    sendData("ERROR:INVALID_FILENAME");
    return false;
  }
  
  if (!filename.endsWith(".mid") && !filename.endsWith(".MID")) {
    sendData("ERROR:NOT_A_MID_FILE");
    return false;
  }
  
  if (fileSize == 0 || fileSize > MAX_FILE_SIZE) {
    sendData("ERROR:INVALID_FILE_SIZE");
    return false;
  }
  
  if (!fileSizeAvailable(fileSize)) {
    sendData("ERROR:INSUFFICIENT_SPACE");
    return false;
  }
  
  String fullPath = String(MID_FILE_DIR) + filename;
  if (SPIFFS.exists(fullPath)) {
    SPIFFS.remove(fullPath);
    Serial.println("[BT] Overwrote existing file: " + fullPath);
  }
  
  currentFile = SPIFFS.open(fullPath, "w");
  if (!currentFile) {
    Serial.println("[BT] Failed to open file for writing: " + fullPath);
    sendData("ERROR:CANNOT_CREATE_FILE");
    return false;
  }
  
  currentFilename = filename;
  expectedFileSize = fileSize;
  bytesReceived = 0;
  fileChecksum = 0;
  ftState = FT_RECEIVING;
  
  Serial.print("[BT] Starting file transfer: ");
  Serial.print(filename);
  Serial.print(" (");
  Serial.print(fileSize);
  Serial.println(" bytes)");
  
  return true;
}

bool BluetoothController::receiveFileData(const uint8_t* buffer, size_t length) {
  if (ftState != FT_RECEIVING || !currentFile) {
    return false;
  }
  
  size_t written = currentFile.write(buffer, length);
  if (written != length) {
    Serial.println("[BT] Write error: wrote " + String(written) + "/" + String(length) + " bytes");
    ftState = FT_ERROR;
    return false;
  }
  
  fileChecksum += calculateChecksum(buffer, length);
  bytesReceived += length;
  
  // Progress logging
  if (bytesReceived % 10240 == 0 || bytesReceived == expectedFileSize) {
    float progress = (float)bytesReceived / expectedFileSize * 100.0;
    sendData("PROGRESS:" + String((int)progress) + "%");
    Serial.print("[BT] Transfer progress: ");
    Serial.print((int)progress);
    Serial.println("%");
  }
  
  return true;
}

bool BluetoothController::endFileTransfer(uint32_t checksum) {
  if (ftState != FT_RECEIVING || !currentFile) {
    ftState = FT_ERROR;
    return false;
  }
  
  currentFile.close();
  
  if (bytesReceived != expectedFileSize) {
    Serial.print("[BT] File size mismatch: received ");
    Serial.print(bytesReceived);
    Serial.print(" of ");
    Serial.println(expectedFileSize);
    
    String fullPath = String(MID_FILE_DIR) + currentFilename;
    SPIFFS.remove(fullPath);
    ftState = FT_ERROR;
    return false;
  }
  
  if (fileChecksum != checksum) {
    Serial.print("[BT] Checksum mismatch: expected ");
    Serial.print(checksum);
    Serial.print(", got ");
    Serial.println(fileChecksum);
    
    String fullPath = String(MID_FILE_DIR) + currentFilename;
    SPIFFS.remove(fullPath);
    ftState = FT_ERROR;
    return false;
  }
  
  ftState = FT_COMPLETE;
  lastTransferredFilename = String(MID_FILE_DIR) + currentFilename;
  newFileTransferFlag = true;
  Serial.print("[BT] File transfer complete: ");
  Serial.println(currentFilename);
  
  return true;
}

void BluetoothController::cancelFileTransfer() {
  if (currentFile) {
    currentFile.close();
  }
  
  if (ftState == FT_RECEIVING) {
    String fullPath = String(MID_FILE_DIR) + currentFilename;
    SPIFFS.remove(fullPath);
    Serial.println("[BT] File transfer cancelled");
  }
  
  ftState = FT_IDLE;
  bytesReceived = 0;
  expectedFileSize = 0;
  fileChecksum = 0;
  currentFilename = "";
}

uint32_t BluetoothController::calculateChecksum(const uint8_t* data, size_t length) {
  uint32_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return sum;
}

void BluetoothController::listMIDFiles() {
  File root = SPIFFS.open(MID_FILE_DIR);
  if (!root) {
    sendData("ERROR:Cannot open directory");
    return;
  }
  
  sendData("=== MIDI Files ===");
  bool foundFiles = false;
  
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      foundFiles = true;
      String filename = file.name();
      size_t fileSize = file.size();
      sendData(filename + " (" + String(fileSize) + " bytes)");
    }
    file = root.openNextFile();
  }
  
  if (!foundFiles) {
    sendData("No MIDI files found");
  }
  
  root.close();
}

bool BluetoothController::deleteMIDFile(const char* filename) {
  String fullPath = String(MID_FILE_DIR) + filename;
  
  if (!SPIFFS.exists(fullPath)) {
    Serial.println("[BT] File not found: " + fullPath);
    return false;
  }
  
  if (SPIFFS.remove(fullPath)) {
    Serial.println("[BT] Deleted: " + fullPath);
    return true;
  }
  
  Serial.println("[BT] Failed to delete: " + fullPath);
  return false;
}

String BluetoothController::getLastMIDFile() {
  File root = SPIFFS.open(MID_FILE_DIR);
  if (!root) return "";
  
  String lastFile = "";
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      lastFile = file.name();
      break;
    }
    file = root.openNextFile();
  }
  
  root.close();
  return lastFile;
}

bool BluetoothController::fileSizeAvailable(uint32_t requiredSize) {
  uint32_t usedBytes = SPIFFS.usedBytes();
  uint32_t totalBytes = SPIFFS.totalBytes();
  uint32_t availableBytes = totalBytes - usedBytes;
  return (availableBytes - 10240) > requiredSize;
}

bool BluetoothController::validateMIDFile(const char* filename) {
  String fullPath = String(MID_FILE_DIR) + filename;
  if (!SPIFFS.exists(fullPath)) {
    return false;
  }
  
  File file = SPIFFS.open(fullPath, "r");
  if (!file) return false;
  
  uint8_t header[4];
  if (file.read(header, 4) != 4) {
    file.close();
    return false;
  }
  
  file.close();
  return (header[0] == 'M' && header[1] == 'T' && 
          header[2] == 'h' && header[3] == 'd');
}

String BluetoothController::checkNewFileTransfer() {
  if (newFileTransferFlag) {
    newFileTransferFlag = false;
    return lastTransferredFilename;
  }
  return "";
}