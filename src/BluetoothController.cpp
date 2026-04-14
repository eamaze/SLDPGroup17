#include "BluetoothController.h"

BluetoothController::BluetoothController(const char* name) 
  : deviceName(name), isConnected(false), ftState(FT_IDLE), 
    bytesReceived(0), expectedFileSize(0), fileChecksum(0) {
}

void BluetoothController::begin() {
  // Initialize SPIFFS for file storage
  initSPIFFS();
  
  if (!serialBT.begin(deviceName)) {
    Serial.println("[BT] Failed to initialize Bluetooth");
  } else {
    Serial.print("[BT] Bluetooth initialized as: ");
    Serial.println(deviceName);
    Serial.println("[BT] Device ready to accept connections");
  }
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
  
  // Print file system info
  uint32_t totalBytes = SPIFFS.totalBytes();
  uint32_t usedBytes = SPIFFS.usedBytes();
  Serial.print("[FS] Total: ");
  Serial.print(totalBytes);
  Serial.print(" bytes, Used: ");
  Serial.print(usedBytes);
  Serial.println(" bytes");
}

bool BluetoothController::isConnectedToBT() {
  return serialBT.hasClient();
}

void BluetoothController::sendData(const String& data) {
  if (serialBT.hasClient()) {
    serialBT.println(data);
  }
}

void BluetoothController::sendPitchData(float pitch, float targetFreq, bool isHit) {
  if (serialBT.hasClient()) {
    String message = String(pitch, 2) + "Hz";
    if (isHit) {
      message += " [HIT!]";
    }
    serialBT.println(message);
  }
}

void BluetoothController::handleIncomingData() {
  if (serialBT.hasClient() && serialBT.available()) {
    String command = readLine();
    
    if (command.length() > 0) {
      Serial.print("[BT] Received: ");
      Serial.println(command);
      
      // Check for file transfer commands
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
        sendData("ESP32 Musical Note Detector v2.0 (with file transfer)");
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
  }
  
  // Handle binary file data if we're in receive mode
  if (ftState == FT_RECEIVING && serialBT.hasClient() && serialBT.available()) {
    handleIncomingBinaryData();
  }
}

void BluetoothController::handleIncomingBinaryData() {
  const size_t BUFFER_SIZE = 256;
  uint8_t buffer[BUFFER_SIZE];
  size_t bytesToRead = 0;
  
  // Calculate how many bytes we still need to receive
  uint32_t remainingBytes = expectedFileSize - bytesReceived;
  
  if (remainingBytes == 0) {
    ftState = FT_ERROR;
    return;
  }
  
  // Read up to BUFFER_SIZE bytes or remaining bytes, whichever is smaller
  bytesToRead = (remainingBytes < BUFFER_SIZE) ? remainingBytes : BUFFER_SIZE;
  
  // Peek at available data without removing it (to avoid blocking)
  size_t available = serialBT.available();
  if (available == 0) {
    return;
  }
  
  bytesToRead = (available < bytesToRead) ? available : bytesToRead;
  
  // Read the actual data
  size_t bytesRead = serialBT.readBytes(buffer, bytesToRead);
  
  if (bytesRead > 0) {
    if (!receiveFileData(buffer, bytesRead)) {
      cancelFileTransfer();
      sendData("ERROR:WRITE_FAILED");
    }
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
  // Validate filename
  if (filename.length() == 0 || filename.length() > MAX_FILENAME_LENGTH) {
    sendData("ERROR:INVALID_FILENAME");
    return false;
  }
  
  // Validate .mid extension
  if (!filename.endsWith(".mid") && !filename.endsWith(".MID")) {
    sendData("ERROR:NOT_A_MID_FILE");
    return false;
  }
  
  // Check file size
  if (fileSize == 0 || fileSize > MAX_FILE_SIZE) {
    sendData("ERROR:INVALID_FILE_SIZE");
    return false;
  }
  
  // Check available space
  if (!fileSizeAvailable(fileSize)) {
    sendData("ERROR:INSUFFICIENT_SPACE");
    return false;
  }
  
  // Delete existing file if it exists
  String fullPath = String(MID_FILE_DIR) + filename;
  if (SPIFFS.exists(fullPath)) {
    SPIFFS.remove(fullPath);
    Serial.println("[BT] Overwrote existing file: " + fullPath);
  }
  
  // Open file for writing
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
  
  // Write data to file
  size_t written = currentFile.write(buffer, length);
  if (written != length) {
    Serial.println("[BT] Write error: wrote " + String(written) + "/" + String(length) + " bytes");
    ftState = FT_ERROR;
    return false;
  }
  
  // Update checksum
  fileChecksum += calculateChecksum(buffer, length);
  
  bytesReceived += length;
  
  // Send progress update every 10KB
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
  
  // Verify file size
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
  
  // Verify checksum
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
  time_t lastTime = 0;
  
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      // Return the first (most recent) file found
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
  
  // Leave 10KB buffer
  return (availableBytes - 10240) > requiredSize;
}

bool BluetoothController::validateMIDFile(const char* filename) {
  String fullPath = String(MID_FILE_DIR) + filename;
  
  if (!SPIFFS.exists(fullPath)) {
    return false;
  }
  
  // Basic MIDI file validation: check for "MThd" header
  File file = SPIFFS.open(fullPath, "r");
  if (!file) return false;
  
  uint8_t header[4];
  if (file.read(header, 4) != 4) {
    file.close();
    return false;
  }
  
  file.close();
  
  // Check for MIDI header "MThd"
  return (header[0] == 'M' && header[1] == 'T' && 
          header[2] == 'h' && header[3] == 'd');
}

String BluetoothController::readLine() {
  String line = "";
  while (serialBT.available()) {
    char c = serialBT.read();
    if (c == '\n' || c == '\r') {
      if (line.length() > 0) {
        return line;
      }
    } else {
      line += c;
    }
  }
  return line;
}
