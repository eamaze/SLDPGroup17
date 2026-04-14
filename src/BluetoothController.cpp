#include "BluetoothController.h"

// --- BLE Callbacks ---

// Callback class for Server connection/disconnection
class MyServerCallbacks : public BLEServerCallbacks {
    BluetoothController* controller;
public:
    MyServerCallbacks(BluetoothController* pController) : controller(pController) {}
    
    void onConnect(BLEServer* pServer) override {
        controller->setConnectionState(true);
    };

    void onDisconnect(BLEServer* pServer) override {
        controller->setConnectionState(false);
    }
};

// Callback class for handling incoming RX data
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
    BluetoothController* controller;
public:
    MyCharacteristicCallbacks(BluetoothController* pController) : controller(pController) {}

    void onWrite(BLECharacteristic* pCharacteristic) override {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            controller->processReceivedData((uint8_t*)rxValue.data(), rxValue.length());
        }
    }
};


// --- Constructor ---
BluetoothController::BluetoothController(const char* name) : 
    deviceName(name),
    pServer(nullptr),
    pTxCharacteristic(nullptr),
    deviceConnected(false),
    oldDeviceConnected(false),
    ftState(FT_IDLE),
    bytesReceived(0),
    expectedFileSize(0),
    fileChecksum(0),
    newFileTransferFlag(false) {
}


// --- Core BLE & System Initialization ---
void BluetoothController::begin() {
    initSPIFFS();

    // Initialize BLE Device
    BLEDevice::init(deviceName);

    // Create BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks(this));

    // Create BLE Service (Nordic UART)
    BLEService* pService = pServer->createService(SERVICE_UUID);

    // Create TX Characteristic (ESP32 -> App)
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // Create RX Characteristic (App -> ESP32)
    BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new MyCharacteristicCallbacks(this));

    // Start Service
    pService->start();

    // Start Advertising
    // CRITICAL FOR IOS: We must advertise the specific Service UUID
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // Help with iPhone connection issues
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLE Started. Waiting for a connection...");
}

void BluetoothController::initSPIFFS() {
    // True = format on fail. Good for first-time use.
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully.");
}


// --- Connection Management ---
bool BluetoothController::isConnectedToBT() {
    return deviceConnected;
}

void BluetoothController::setConnectionState(bool state) {
    deviceConnected = state;
    if (state) {
        Serial.println("Device Connected!");
    } else {
        Serial.println("Device Disconnected!");
    }
}

void BluetoothController::handleIncomingData() {
    // Handle disconnect event: restart advertising so iOS can see us again
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // Give the bluetooth stack the chance to get things ready
        pServer->startAdvertising();
        Serial.println("Restarted Advertising.");
        oldDeviceConnected = deviceConnected;
        
        // Clean up any pending file transfers
        if (ftState == FT_RECEIVING) {
            cancelFileTransfer();
        }
    }
    
    // Handle connect event
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
}


// --- Outgoing Data Transmissions ---
void BluetoothController::sendData(const String& data) {
    if (deviceConnected && pTxCharacteristic != nullptr) {
        pTxCharacteristic->setValue(data.c_str());
        pTxCharacteristic->notify();
    }
}

void BluetoothController::sendPitchData(float pitch, float targetFreq, bool isHit) {
    // Example format: PITCH:440.5|TARGET:440.0|HIT:1
    String payload = "PITCH:" + String(pitch, 2) + 
                     "|TARGET:" + String(targetFreq, 2) + 
                     "|HIT:" + (isHit ? "1" : "0");
    sendData(payload);
}


// --- Incoming Data & Command Parsing ---
void BluetoothController::processReceivedData(uint8_t* data, size_t length) {
    if (ftState == FT_RECEIVING) {
        receiveFileData(data, length);
    } else {
        // If not receiving a file, treat incoming payload as a text command
        String command = "";
        for (size_t i = 0; i < length; i++) {
            command += (char)data[i];
        }
        handleFileTransferCommand(command);
    }
}

void BluetoothController::handleFileTransferCommand(const String& command) {
    // Expected Commands:
    // START|<filename>|<filesize>
    // END|<checksum>
    // CANCEL
    // LIST
    
    if (command.startsWith("START|")) {
        int firstPipe = command.indexOf('|');
        int secondPipe = command.indexOf('|', firstPipe + 1);
        
        if (firstPipe > 0 && secondPipe > 0) {
            String filename = command.substring(firstPipe + 1, secondPipe);
            uint32_t fileSize = command.substring(secondPipe + 1).toInt();
            
            if (startFileTransfer(filename, fileSize)) {
                sendData("ACK_START");
            } else {
                sendData("ERR_START");
            }
        }
    } 
    else if (command.startsWith("END|")) {
        int firstPipe = command.indexOf('|');
        uint32_t checksum = command.substring(firstPipe + 1).toInt();
        
        if (endFileTransfer(checksum)) {
            sendData("ACK_END");
        } else {
            sendData("ERR_CHECKSUM");
        }
    } 
    else if (command == "CANCEL") {
        cancelFileTransfer();
        sendData("ACK_CANCEL");
    }
    else if (command == "LIST") {
        listMIDFiles();
    }
}


// --- File Transfer Logic ---
bool BluetoothController::startFileTransfer(const String& filename, uint32_t fileSize) {
    if (fileSize > MAX_FILE_SIZE || !fileSizeAvailable(fileSize)) {
        Serial.println("File too large or not enough space.");
        return false;
    }

    if (!validateMIDFile(filename.c_str())) {
        Serial.println("Invalid file extension. Must be .mid");
        return false;
    }

    // Prepare full path
    String fullPath = String(MID_FILE_DIR);
    if (!filename.startsWith("/")) { fullPath += "/"; }
    fullPath += filename;

    currentFile = SPIFFS.open(fullPath, FILE_WRITE);
    if (!currentFile) {
        Serial.println("Failed to open file for writing");
        return false;
    }

    currentFilename = fullPath;
    expectedFileSize = fileSize;
    bytesReceived = 0;
    fileChecksum = 0; // Reset calculated checksum
    ftState = FT_RECEIVING;
    
    Serial.printf("Starting transfer: %s (%d bytes)\n", fullPath.c_str(), fileSize);
    return true;
}

bool BluetoothController::receiveFileData(const uint8_t* buffer, size_t length) {
    if (ftState != FT_RECEIVING || !currentFile) return false;

    size_t bytesWritten = currentFile.write(buffer, length);
    if (bytesWritten != length) {
        Serial.println("File write failed or partial write");
        cancelFileTransfer();
        return false;
    }

    // Accumulate a simple additive checksum for validation
    fileChecksum += calculateChecksum(buffer, length);
    bytesReceived += length;

    // Check if we hit the expected file size
    if (bytesReceived >= expectedFileSize) {
        Serial.println("Expected bytes received. Waiting for END command.");
        // We stay in FT_RECEIVING until the END command validates it
    }
    return true;
}

bool BluetoothController::endFileTransfer(uint32_t checksum) {
    if (ftState != FT_RECEIVING) return false;

    if (currentFile) {
        currentFile.close();
    }

    // Validate checksum and size
    if (bytesReceived == expectedFileSize && checksum == fileChecksum) {
        ftState = FT_COMPLETE;
        lastTransferredFilename = currentFilename;
        newFileTransferFlag = true;
        Serial.println("File transfer complete and verified.");
        return true;
    } else {
        Serial.println("File transfer failed verification.");
        SPIFFS.remove(currentFilename); // Delete corrupted file
        ftState = FT_ERROR;
        return false;
    }
}

void BluetoothController::cancelFileTransfer() {
    if (currentFile) {
        currentFile.close();
    }
    if (currentFilename.length() > 0) {
        SPIFFS.remove(currentFilename);
    }
    ftState = FT_IDLE;
    bytesReceived = 0;
    expectedFileSize = 0;
    Serial.println("File transfer cancelled.");
}


// --- File Management ---
void BluetoothController::listMIDFiles() {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    
    String fileList = "FILES:";
    while (file) {
        String fname = file.name();
        if (fname.indexOf(".mid") > 0 || fname.indexOf(".MID") > 0) {
            fileList += fname + "," + String(file.size()) + ";";
        }
        file = root.openNextFile();
    }
    sendData(fileList);
}

bool BluetoothController::deleteMIDFile(const char* filename) {
    String fullPath = String(filename);
    if (!fullPath.startsWith("/")) fullPath = "/" + fullPath;
    
    if (SPIFFS.exists(fullPath)) {
        return SPIFFS.remove(fullPath);
    }
    return false;
}

String BluetoothController::getLastMIDFile() {
    return lastTransferredFilename;
}

bool BluetoothController::fileSizeAvailable(uint32_t requiredSize) {
    uint32_t totalBytes = SPIFFS.totalBytes();
    uint32_t usedBytes = SPIFFS.usedBytes();
    return (totalBytes - usedBytes) > requiredSize;
}


// --- State Checks ---
String BluetoothController::checkNewFileTransfer() {
    if (newFileTransferFlag) {
        newFileTransferFlag = false;
        ftState = FT_IDLE; // Reset state back to idle after acknowledging
        return lastTransferredFilename;
    }
    return "";
}

FileTransferState BluetoothController::getFileTransferState() const {
    return ftState;
}

String BluetoothController::getLastTransferredFile() const {
    return lastTransferredFilename;
}


// --- Private Helpers ---
bool BluetoothController::validateMIDFile(const char* filename) {
    String fname = String(filename);
    fname.toLowerCase();
    return fname.endsWith(".mid");
}

uint32_t BluetoothController::calculateChecksum(const uint8_t* data, size_t length) {
    // Simple additive checksum modulo 32-bit limit
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}