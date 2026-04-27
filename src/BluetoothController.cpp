#include "BluetoothController.h"

BluetoothController::BluetoothController(const char* name)
    : deviceName(name),
      apSsid("Keystroke-ESP8266"),
      apPassword("keystroke123"),
      ws(81),
      deviceConnected(false),
      ftState(FT_IDLE),
      bytesReceived(0),
      expectedFileSize(0),
      fileChecksum(0),
      newFileTransferFlag(false),
      currentBPM(120),
      startCommandFlag(false),
      activeClient(255)
{
}

void BluetoothController::setApCredentials(const char* ssid, const char* password) {
    apSsid = ssid;
    apPassword = password;
}

void BluetoothController::initFS() {
    if (!LittleFS.begin()) {
        Serial.println("[FS] LittleFS mount failed. Formatting...");
        LittleFS.format();
        if (!LittleFS.begin()) {
            Serial.println("[FS] LittleFS mount failed after format");
        }
    }

    // Ensure midi directory exists (LittleFS uses paths without true dirs, but we can still use prefix)
}

void BluetoothController::begin() {
    initFS();

    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(apSsid, apPassword);
    if (!ok) {
        Serial.println("[WiFi] softAP failed");
    }

    IPAddress ip = WiFi.softAPIP();
    Serial.print("[WiFi] AP started. SSID=");
    Serial.print(apSsid);
    Serial.print(" IP=");
    Serial.println(ip);

    ws.begin();
    ws.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        this->handleWsEvent(num, type, payload, length);
    });

    Serial.println("[WS] WebSocket server listening on ws://192.168.4.1:81/");
}

bool BluetoothController::isConnectedToBT() {
    return deviceConnected;
}

void BluetoothController::setConnectionState(bool state) {
    deviceConnected = state;
}

void BluetoothController::handleIncomingData() {
    ws.loop();
}

void BluetoothController::sendData(const String& data) {
    if (!deviceConnected) return;
    if (activeClient == 255) return;
    ws.sendTXT(activeClient, data);
}

void BluetoothController::sendSongCompleted() {
    sendData("SONG_COMPLETED");
}

void BluetoothController::processReceivedData(uint8_t* data, size_t length) {
    // Not used in WS implementation; kept for API compatibility
    (void)data;
    (void)length;
}

void BluetoothController::handleWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED: {
            // Enforce single-client to reduce RAM
            if (activeClient != 255 && activeClient != num) {
                ws.disconnect(activeClient);
            }
            activeClient = num;
            deviceConnected = true;
            Serial.printf("[WS] Client %u connected\n", num);
            sendData("HELLO");
            break;
        }
        case WStype_DISCONNECTED: {
            if (activeClient == num) {
                activeClient = 255;
                deviceConnected = false;
            }
            Serial.printf("[WS] Client %u disconnected\n", num);
            if (ftState == FT_RECEIVING) {
                cancelFileTransfer();
            }
            break;
        }
        case WStype_TEXT: {
            String cmd;
            cmd.reserve(length + 1);
            for (size_t i = 0; i < length; i++) cmd += (char)payload[i];
            cmd.trim();
            handleFileTransferCommand(cmd);
            break;
        }
        case WStype_BIN: {
            // Treat as file data if receiving
            if (ftState == FT_RECEIVING && bytesReceived < expectedFileSize) {
                receiveFileData(payload, length);
            }
            break;
        }
        default:
            break;
    }
}

void BluetoothController::handleFileTransferCommand(const String& command) {
    if (command == "CANCEL") {
        cancelFileTransfer();
        sendData("ACK_CANCEL");
        return;
    }

    if (command == "LIST") {
        listMIDFiles();
        return;
    }

    if (command.startsWith("BPM:")) {
        currentBPM = command.substring(4).toInt();
        if (currentBPM == 0) currentBPM = 120;
        Serial.printf("[WS] BPM=%u\n", currentBPM);
        return;
    }

    if (command == "BEGINSONG") {
        startCommandFlag = true;
        Serial.println("[WS] BEGINSONG");
        return;
    }

    if (command.startsWith("START|")) {
        int firstPipe = command.indexOf('|');
        int secondPipe = command.indexOf('|', firstPipe + 1);
        if (firstPipe > 0 && secondPipe > 0) {
            String filename = command.substring(firstPipe + 1, secondPipe);
            uint32_t fileSize = command.substring(secondPipe + 1).toInt();
            if (startFileTransfer(filename, fileSize)) sendData("ACK_START");
            else sendData("ERR_START");
        }
        return;
    }

    if (command.startsWith("END|")) {
        int firstPipe = command.indexOf('|');
        uint32_t checksum = command.substring(firstPipe + 1).toInt();
        if (endFileTransfer(checksum)) sendData("ACK_END");
        else sendData("ERR_CHECKSUM");
        return;
    }
}

bool BluetoothController::validateMIDFile(const char* filename) {
    String fname(filename);
    fname.toLowerCase();
    return fname.endsWith(".mid");
}

uint32_t BluetoothController::calculateChecksum(const uint8_t* data, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) sum += data[i];
    return sum;
}

bool BluetoothController::fileSizeAvailable(uint32_t requiredSize) {
    FSInfo info;
    LittleFS.info(info);
    uint32_t freeBytes = info.totalBytes - info.usedBytes;
    return freeBytes > requiredSize;
}

bool BluetoothController::startFileTransfer(const String& filename, uint32_t fileSize) {
    if (fileSize > MAX_FILE_SIZE || !fileSizeAvailable(fileSize)) {
        Serial.println("[FS] File too large or not enough space");
        return false;
    }

    if (!validateMIDFile(filename.c_str())) {
        Serial.println("[FS] Invalid file extension (.mid required)");
        return false;
    }

    // LittleFS doesn't have real directories; keep it as /midi_<name>.mid
    String safe = filename;
    safe.replace("/", "_");

    String fullPath = String(MID_FILE_DIR) + "_" + safe;
    if (!fullPath.startsWith("/")) fullPath = "/" + fullPath;

    currentFile = LittleFS.open(fullPath, "w");
    if (!currentFile) {
        Serial.println("[FS] Failed to open file for writing");
        return false;
    }

    currentFilename = fullPath;
    expectedFileSize = fileSize;
    bytesReceived = 0;
    fileChecksum = 0;
    ftState = FT_RECEIVING;

    Serial.printf("[FS] START %s (%u bytes)\n", fullPath.c_str(), (unsigned)fileSize);
    return true;
}

bool BluetoothController::receiveFileData(const uint8_t* buffer, size_t length) {
    if (ftState != FT_RECEIVING || !currentFile) return false;

    size_t written = currentFile.write(buffer, length);
    if (written != length) {
        Serial.println("[FS] Write failed");
        cancelFileTransfer();
        return false;
    }

    fileChecksum += calculateChecksum(buffer, length);
    bytesReceived += length;

    return true;
}

bool BluetoothController::endFileTransfer(uint32_t checksum) {
    if (ftState != FT_RECEIVING) return false;

    if (currentFile) currentFile.close();

    if (bytesReceived == expectedFileSize && checksum == fileChecksum) {
        ftState = FT_COMPLETE;
        lastTransferredFilename = currentFilename;
        newFileTransferFlag = true;
        Serial.println("[FS] Transfer complete OK");
        return true;
    }

    Serial.println("[FS] Transfer verification failed");
    LittleFS.remove(currentFilename);
    ftState = FT_ERROR;
    return false;
}

void BluetoothController::cancelFileTransfer() {
    if (currentFile) currentFile.close();
    if (currentFilename.length() > 0) LittleFS.remove(currentFilename);

    ftState = FT_IDLE;
    bytesReceived = 0;
    expectedFileSize = 0;
    currentFilename = "";
    Serial.println("[FS] Transfer cancelled");
}

void BluetoothController::listMIDFiles() {
    // LittleFS doesn't provide efficient directory iteration in the same way across cores.
    // Provide a minimal response; app can track uploads or we can implement a simple index file later.
    FSInfo info;
    LittleFS.info(info);
    String resp = "FILES_UNSUPPORTED;FREE=" + String(info.totalBytes - info.usedBytes);
    sendData(resp);
}

bool BluetoothController::deleteMIDFile(const char* filename) {
    String path(filename);
    if (!path.startsWith("/")) path = "/" + path;
    return LittleFS.remove(path);
}

String BluetoothController::getLastMIDFile() {
    return lastTransferredFilename;
}

String BluetoothController::checkNewFileTransfer() {
    if (newFileTransferFlag) {
        newFileTransferFlag = false;
        ftState = FT_IDLE;
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

uint16_t BluetoothController::getBPM() const {
    return currentBPM;
}

bool BluetoothController::checkStartCommand() {
    if (startCommandFlag) {
        startCommandFlag = false;
        return true;
    }
    return false;
}
