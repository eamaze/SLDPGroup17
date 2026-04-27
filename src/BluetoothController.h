#ifndef BLUETOOTH_CONTROLLER_H
#define BLUETOOTH_CONTROLLER_H

#include <Arduino.h>

#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>

// Keep existing path/limits (but stored in LittleFS)
#define MID_FILE_DIR "/midi"
#define MAX_FILENAME_LENGTH 32
#define MAX_FILE_SIZE 262144  // ESP8266: reduce max to 256KB to avoid FS/memory issues

// File transfer states
enum FileTransferState {
  FT_IDLE,
  FT_RECEIVING,
  FT_COMPLETE,
  FT_ERROR
};

/**
 * ESP8266 implementation:
 * - Runs AP mode
 * - WebSocket server on port 81
 * - Binary frames are treated as file data when FT_RECEIVING
 * - Text frames are treated as commands (START|..., END|..., LIST, CANCEL, BPM:, BEGINSONG)
 */
class BluetoothController {
private:
    // Kept name for compatibility with existing code
    const char* deviceName;

    // WiFi AP
    const char* apSsid;
    const char* apPassword;

    // WebSocket server
    WebSocketsServer ws;
    bool deviceConnected;

    // File transfer management
    FileTransferState ftState;
    File currentFile;
    String currentFilename;
    uint32_t bytesReceived;
    uint32_t expectedFileSize;
    uint32_t fileChecksum;
    bool newFileTransferFlag;
    String lastTransferredFilename;

    // Playback state management
    uint16_t currentBPM;
    bool startCommandFlag;

    // Private helper methods
    void initFS();
    bool validateMIDFile(const char* filename);
    uint32_t calculateChecksum(const uint8_t* data, size_t length);
    void handleFileTransferCommand(const String& command);
    void handleWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

    // Single-client policy
    uint8_t activeClient;

public:
    BluetoothController(const char* name = "KEYSTROKE-DEVICE");

    // Configure AP credentials (optional; defaults are provided)
    void setApCredentials(const char* ssid, const char* password);

    void begin();

    bool isConnectedToBT();

    // NOTE: take String by VALUE to avoid binding issues with temporaries on some cores
    void sendData(String data);
    void sendSongCompleted();

    void handleIncomingData();

    // Compatibility no-ops / kept API
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

    // State checkers for main loop
    String checkNewFileTransfer();
    FileTransferState getFileTransferState() const;
    String getLastTransferredFile() const;

    // Playback state accessors
    uint16_t getBPM() const;
    bool checkStartCommand();
};

#endif
