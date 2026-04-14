#ifndef BLUETOOTH_CONTROLLER_H
#define BLUETOOTH_CONTROLLER_H

#include <Arduino.h>
#include <SPIFFS.h>

// Include BLE Libraries
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define MID_FILE_DIR "/midi/"
#define MAX_FILENAME_LENGTH 32
#define MAX_FILE_SIZE 524288  // 512KB max file size

// Standard Nordic UART Service UUIDs
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// File transfer states
enum FileTransferState {
  FT_IDLE,
  FT_RECEIVING,
  FT_COMPLETE,
  FT_ERROR
};

class BluetoothController {
private:
    const char* deviceName;
    
    // BLE specific variables
    BLEServer* pServer;
    BLECharacteristic* pTxCharacteristic;
    bool deviceConnected;
    bool oldDeviceConnected;
    
    // Buffer for incoming commands
    String commandQueue;
    
    // File transfer management
    FileTransferState ftState;
    File currentFile;
    String currentFilename;
    uint32_t bytesReceived;
    uint32_t expectedFileSize;
    uint32_t fileChecksum;
    bool newFileTransferFlag;  // Flag when file transfer completes
    String lastTransferredFilename;
    
    // Private helper methods
    void initSPIFFS();
    bool validateMIDFile(const char* filename);
    uint32_t calculateChecksum(const uint8_t* data, size_t length);
    void handleFileTransferCommand(const String& command);

public:
    BluetoothController(const char* name = "ESP32_MusicalNote");
    
    void begin();
    
    bool isConnectedToBT();
    
    void sendData(const String& data);
    void sendPitchData(float pitch, float targetFreq, bool isHit);
    
    void handleIncomingData();
    
    // Callbacks for BLE Server & Characteristics
    void setConnectionState(bool state);
    void processReceivedData(uint8_t* data, size_t length);
    
    // File transfer methods
    bool startFileTransfer(const String& filename, uint32_t fileSize);
    bool receiveFileData(const uint8_t* buffer, size_t length);
    bool endFileTransfer(uint32_t checksum);
    void cancelFileTransfer();
    
    // File management methods
    void listMIDFiles();
    bool deleteMIDFile(const char* filename);
    String getLastMIDFile();
    bool fileSizeAvailable(uint32_t requiredSize);
    
    // Check if a new file was just transferred (and clear the flag)
    String checkNewFileTransfer();
    
    // Check current file transfer state
    FileTransferState getFileTransferState() const {
        return ftState;
    }
    
    // Get the last successfully transferred file
    String getLastTransferredFile() const {
        return currentFilename;
    }
};

#endif