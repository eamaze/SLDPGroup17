import Foundation
import Combine
import CoreBluetooth

final class BLEManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    // MARK: - Public published state
    @Published var isBluetoothOn: Bool = false
    @Published var isScanning: Bool = false
    @Published var isConnected: Bool = false
    @Published var deviceName: String? = nil
    @Published var status: String = "Idle"
    @Published var lastError: String? = nil
    @Published var discoveredDevices: [(peripheral: CBPeripheral, name: String)] = []

    // Optional: publish incoming text lines from ESP32 TX notifications
    @Published var incomingLines: [String] = []

    // MARK: - CoreBluetooth
    private var centralManager: CBCentralManager!
    private var espPeripheral: CBPeripheral?

    // NUS (Nordic UART Service) UUIDs (match BluetoothController.h)
    private let nusServiceUUID = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    private let nusRXUUID = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E") // write to ESP32
    private let nusTXUUID = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E") // notify from ESP32

    private var rxCharacteristic: CBCharacteristic?
    private var txCharacteristic: CBCharacteristic?

    private let rssiThreshold: Int = -70

    // MARK: - File transfer protocol selection
    enum FileTransferProtocol {
        /// Legacy protocol in your earlier ESP32 code:
        /// FILE_START:<filename>:<filesize>\n ... FILE_END:<checksum>\n
        case colon

        /// Updated protocol you posted:
        /// START|<filename>|<filesize>\n ... END|<checksum>\n
        case pipe
    }

    /// Set this to match the ESP32 firmware currently on your board.
    /// If you flashed the new handleFileTransferCommand(START|... / END|...), use `.pipe`.
    var fileTransferProtocol: FileTransferProtocol = .pipe

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    // MARK: - Public API
    func startScan() {
        guard isBluetoothOn else {
            status = "Bluetooth is off"
            return
        }
        guard !isScanning else { return }

        discoveredDevices.removeAll()
        status = "Scanning for ESP32…"
        isScanning = true

        // Scan for the NUS service
        centralManager.scanForPeripherals(withServices: [nusServiceUUID],
                                          options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    func stopScan() {
        guard isScanning else { return }
        centralManager.stopScan()
        isScanning = false
        status = isConnected ? "Connected" : "Idle"
    }

    func disconnect() {
        if let p = espPeripheral {
            centralManager.cancelPeripheralConnection(p)
        }
    }

    func connect(to peripheral: CBPeripheral) {
        stopScan()
        espPeripheral = peripheral
        espPeripheral?.delegate = self
        deviceName = peripheral.name
        status = "Connecting to \(peripheral.name ?? "Unknown")"
        centralManager.connect(peripheral, options: nil)
    }

    /// Sends a UTF-8 string to ESP32 over NUS RX (no newline is added).
    func sendString(_ text: String) {
        guard let peripheral = espPeripheral, let rx = rxCharacteristic else {
            lastError = "Not ready to send (no connection or RX characteristic)"
            return
        }
        guard let data = text.data(using: .utf8) else {
            lastError = "Failed to encode string as UTF-8"
            return
        }
        peripheral.writeValue(data, for: rx, type: .withResponse)
    }

    /// Sends a command line, ensuring it ends in '\n' so ESP32 parses it.
    func sendCommandLine(_ line: String) {
        if line.hasSuffix("\n") || line.hasSuffix("\r") {
            sendString(line)
        } else {
            sendString(line + "\n")
        }
    }

    /// Forces sending END|<checksum> (or FILE_END:<checksum>) as a newline-terminated command.
    func sendEndCommand(checksum: UInt32) {
        switch fileTransferProtocol {
        case .pipe:
            sendCommandLine("END|\(checksum)")
        case .colon:
            sendCommandLine("FILE_END:\(checksum)")
        }
    }

    // MARK: - File transfer (binary) over NUS RX

    /// Sends a binary file using the selected ESP32 protocol.
    /// checksum matches ESP32 calculateChecksum(): sum of bytes (uint32 wrap).
    func sendFile(filename: String, data: Data, chunkSize: Int = 180) {
        guard isConnected else {
            lastError = "Not connected"
            return
        }
        guard !filename.isEmpty else {
            lastError = "Filename is empty"
            return
        }
        guard rxCharacteristic != nil else {
            lastError = "Not ready (missing RX characteristic)"
            return
        }

        let startLine: String
        switch fileTransferProtocol {
        case .colon:
            startLine = "FILE_START:\(filename):\(data.count)"
        case .pipe:
            startLine = "START|\(filename)|\(data.count)"
        }

        status = "Sending \(filename) (\(data.count) bytes)…"
        sendCommandLine(startLine)

        var checksum: UInt32 = 0
        var offset = 0

        while offset < data.count {
            let end = min(offset + chunkSize, data.count)
            let chunk = data.subdata(in: offset..<end)

            for b in chunk {
                checksum &+= UInt32(b) // wrap like uint32 on ESP32
            }

            sendRaw(chunk)
            offset = end
        }

        // Send END command explicitly
        sendEndCommand(checksum: checksum)

        status = "File sent (waiting for ACK)"
    }

    func cancelFileTransfer() {
        // Your newer ESP32 code supports CANCEL as a control command.
        sendCommandLine("CANCEL")
    }

    func listFiles() {
        switch fileTransferProtocol {
        case .colon:
            sendCommandLine("LIST_MIDI")
        case .pipe:
            sendCommandLine("LIST")
        }
    }

    /// Writes raw bytes to the ESP32 RX characteristic (no newline, no UTF-8 encoding).
    private func sendRaw(_ data: Data) {
        guard let peripheral = espPeripheral, let rx = rxCharacteristic else {
            lastError = "Not ready to send (no connection or RX characteristic)"
            return
        }
        peripheral.writeValue(data, for: rx, type: .withResponse)
    }

    // MARK: - CBCentralManagerDelegate
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            isBluetoothOn = true
            status = "Bluetooth On"
        case .poweredOff:
            isBluetoothOn = false
            status = "Bluetooth Off"
        case .unauthorized:
            isBluetoothOn = false
            status = "Bluetooth Unauthorized"
        case .unsupported:
            isBluetoothOn = false
            status = "Bluetooth Unsupported"
        case .resetting:
            isBluetoothOn = false
            status = "Bluetooth Resetting"
        case .unknown:
            fallthrough
        @unknown default:
            isBluetoothOn = false
            status = "Bluetooth Unknown"
        }

        if isBluetoothOn {
            startScan()
        } else {
            stopScan()
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String : Any],
                        rssi RSSI: NSNumber) {
        guard RSSI.intValue >= rssiThreshold else { return }

        let advertisedName = advertisementData[CBAdvertisementDataLocalNameKey] as? String
        let name = advertisedName ?? peripheral.name ?? "Unknown"

        status = "Discovered: \(name)"

        if let idx = discoveredDevices.firstIndex(where: { $0.peripheral.identifier == peripheral.identifier }) {
            if discoveredDevices[idx].name != name {
                discoveredDevices[idx].name = name
            }
        } else {
            discoveredDevices.append((peripheral, name))
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        isConnected = true
        status = "Connected"

        rxCharacteristic = nil
        txCharacteristic = nil

        peripheral.discoverServices([nusServiceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        isConnected = false
        lastError = error?.localizedDescription ?? "Failed to connect"
        status = "Failed to connect"
        startScan()
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        isConnected = false
        rxCharacteristic = nil
        txCharacteristic = nil

        if let err = error {
            lastError = err.localizedDescription
            status = "Disconnected (error)"
        } else {
            status = "Disconnected"
        }

        startScan()
    }

    // MARK: - CBPeripheralDelegate
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error = error {
            lastError = error.localizedDescription
            return
        }
        guard let services = peripheral.services else { return }

        for service in services where service.uuid == nusServiceUUID {
            peripheral.discoverCharacteristics([nusRXUUID, nusTXUUID], for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error = error {
            lastError = error.localizedDescription
            return
        }
        guard service.uuid == nusServiceUUID else { return }
        guard let characteristics = service.characteristics else { return }

        for c in characteristics {
            if c.uuid == nusRXUUID {
                rxCharacteristic = c
            } else if c.uuid == nusTXUUID {
                txCharacteristic = c
                if c.properties.contains(.notify) {
                    peripheral.setNotifyValue(true, for: c)
                }
            }
        }

        if rxCharacteristic != nil && txCharacteristic != nil {
            status = "Ready to send/receive"
        } else {
            status = "Found service, waiting for RX/TX..."
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            lastError = "Write failed: \(error.localizedDescription)"
        } else {
            status = "Write succeeded"
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            lastError = "Notify error: \(error.localizedDescription)"
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            lastError = error.localizedDescription
            return
        }
        guard characteristic.uuid == nusTXUUID else { return }
        guard let data = characteristic.value else { return }

        if let text = String(data: data, encoding: .utf8) {
            let trimmed = text.trimmingCharacters(in: .newlines)
            incomingLines.append(trimmed)
            print("ESP32 TX: \(trimmed)")
        } else {
            print("ESP32 TX (\(data.count) bytes)")
        }
    }
}
