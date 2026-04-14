#ifndef BLUETOOTH_CONTROLLER_H
#define BLUETOOTH_CONTROLLER_H

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <SPIFFS.h>

#define MID_FILE_DIR "/midi/"
#define MAX_FILENAME_LENGTH 32
#define MAX_FILE_SIZE 524288  // 512KB max file size

// File transfer states
enum FileTransferState {
  FT_IDLE,
  FT_RECEIVING,
  FT_COMPLETE,
  FT_ERROR
};

class BluetoothController {
private:
    BluetoothSerial serialBT;
    bool isConnected;
    const char* deviceName;
    
    // File transfer management
    FileTransferState ftState;
    File currentFile;
    String currentFilename;
    uint32_t bytesReceived;
    uint32_t expectedFileSize;
    uint32_t fileChecksum;
    
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
    void handleIncomingBinaryData();
    
    String readLine();
    
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
};

#endif
