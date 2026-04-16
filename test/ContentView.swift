import SwiftUI
import SwiftData
import CoreBluetooth

struct ContentView: View {
    @Environment(\.modelContext) private var modelContext
    @Query private var items: [Item]
    @StateObject private var ble = BLEManager()
    @State private var tempoBPM: Int = 120
    @State private var currentBeat: Int = 0
    @State private var isCountingIn: Bool = false

    var body: some View {
        NavigationSplitView {
            List {
                ForEach(items) { item in
                    NavigationLink {
                        Text("Item at \(item.timestamp, format: Date.FormatStyle(date: .numeric, time: .standard))")
                    } label: {
                        Text(item.timestamp, format: Date.FormatStyle(date: .numeric, time: .standard))
                    }
                }
                .onDelete(perform: deleteItems)
            }
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    EditButton()
                }

                ToolbarItem {
                    Button(action: addItem) {
                        Label("Add Item", systemImage: "plus")
                    }
                }

                // Import MIDI button (local parse / debug)
                ToolbarItem {
                    Button(action: importTestMIDI) {
                        Label("Import MIDI", systemImage: "music.note")
                    }
                }

                // Send raw MIDI file over BLE using the ESP32 pipe protocol
                ToolbarItem {
                    Button(action: sendTestMIDOverBLE) {
                        Label { Text("Send MIDI (\(tempoBPM) BPM)") } icon: { Image(systemName: "paperplane") }
                    }
                    .disabled(!ble.isConnected)
                }

                // Scan BLE button
                ToolbarItem {
                    Button(action: { ble.startScan() }) {
                        Label {
                            Text(ble.isScanning ? "Scanning…" : "Scan BLE")
                        } icon: {
                            Image(systemName: ble.isScanning ? "dot.radiowaves.left.and.right" : "antenna.radiowaves.left.and.right")
                        }
                    }
                    .disabled(!ble.isBluetoothOn || ble.isScanning || ble.isConnected)
                }
            }
        } detail: {
            VStack(spacing: 12) {
                Text("Select an item")
                Divider()

                HStack {
                    Text("Tempo: \(tempoBPM) BPM")
                    Spacer()
                    Stepper("", value: $tempoBPM, in: 30...300)
                        .labelsHidden()
                }

                Group {
                    Text("Bluetooth: \(ble.isBluetoothOn ? "On" : "Off")")
                    Text("Status: \(ble.status)")
                    if let name = ble.deviceName { Text("Device: \(name)") }
                    if let err = ble.lastError { Text("Error: \(err)").foregroundStyle(.red) }
                }

                HStack {
                    Button("Scan") { ble.startScan() }
                        .disabled(!ble.isBluetoothOn || ble.isScanning || ble.isConnected)
                    Button("Stop Scan") { ble.stopScan() }
                        .disabled(!ble.isScanning)
                    Button("Disconnect") { ble.disconnect() }
                        .disabled(!ble.isConnected)
                    Button("Start") { sendStartCommand() }
                        .disabled(!ble.isConnected)
                }

                Divider()

                if isCountingIn {
                    VStack(spacing: 8) {
                        Text("Count-in")
                            .font(.headline)
                        HStack(spacing: 16) {
                            ForEach(1...4, id: \.self) { beat in
                                Circle()
                                    .fill(beat == currentBeat ? Color.accentColor : Color.secondary.opacity(0.3))
                                    .frame(width: 20, height: 20)
                                    .scaleEffect(beat == currentBeat ? 1.3 : 1.0)
                                    .animation(.easeOut(duration: 0.15), value: currentBeat)
                                    .overlay(
                                        Text("\(beat)")
                                            .font(.caption2)
                                            .foregroundStyle(.background)
                                    )
                            }
                        }
                    }
                }

                Divider()

                Text("Discovered Devices")
                    .font(.headline)

                List(ble.discoveredDevices, id: \.peripheral.identifier) { item in
                    Button(action: { ble.connect(to: item.peripheral) }) {
                        HStack {
                            Image(systemName: "antenna.radiowaves.left.and.right")
                            Text(item.name.isEmpty ? "Unknown" : item.name)
                        }
                    }
                }
                .frame(maxHeight: 250)

                Divider()

                // Quick command buttons (optional, helpful for debugging ESP32 responses)
                HStack {
                    Button("LIST") { ble.listFiles() }
                        .disabled(!ble.isConnected)
                    Button("CANCEL") { ble.cancelFileTransfer() }
                        .disabled(!ble.isConnected)
                }

                // Show incoming TX notifications from ESP32 (ACK_START/ACK_END/etc.)
                if !ble.incomingLines.isEmpty {
                    Divider()
                    Text("ESP32 Messages")
                        .font(.headline)

                    List(ble.incomingLines.reversed(), id: \.self) { line in
                        Text(line)
                            .font(.system(.body, design: .monospaced))
                    }
                    .frame(maxHeight: 220)
                }
            }
            .padding()
        }
    }

    private func addItem() {
        withAnimation {
            let newItem = Item(timestamp: Date())
            modelContext.insert(newItem)
        }
    }

    private func deleteItems(offsets: IndexSet) {
        withAnimation {
            for index in offsets {
                modelContext.delete(items[index])
            }
        }
    }

    // Local MIDI parse / debug (no BLE)
    private func importTestMIDI() {
        guard let url = Bundle.main.url(forResource: "testMID", withExtension: "mid") else {
            print("Could not find test.mid in app bundle (make sure it's added to the target).")
            return
        }

        do {
            let notes = try parseMIDINotesOnly(from: url)
            print("Parsed \(notes.count) notes from test.mid")
            for n in notes.prefix(20) {
                print("startBeat=\(n.startBeat) durBeats=\(n.durationBeats) note=\(n.note) vel=\(n.velocity) ch=\(n.channel)")
            }
        } catch {
            print("MIDI parse failed:", error)
        }
    }

    private func sendTestMIDOverBLE() {
        guard ble.isConnected else { return }
        guard let url = Bundle.main.url(forResource: "testMID", withExtension: "mid") else { return }

        do {
            let midiData = try Data(contentsOf: url)

            // 1) Send a small metadata JSON that includes tempo.
            let meta: [String: Any] = [
                "filename": "testMID.mid",
                "tempoBPM": tempoBPM
            ]
            let metaData = try JSONSerialization.data(withJSONObject: meta, options: [])
            ble.fileTransferProtocol = .pipe   // IMPORTANT: matches START| / END|
            ble.sendFile(filename: "testMID.meta.json", data: metaData)

            // 2) Send the MIDI file.
            ble.sendFile(filename: "testMID.mid", data: midiData)

        } catch {
            print("Failed to prepare/send files:", error)
        }
    }

    private func sendStartCommand() {
        guard ble.isConnected else { return }
        // Send a small control payload using the same pipe/file transfer mechanism
        let command = "START"
        if let data = command.data(using: .utf8) {
            ble.fileTransferProtocol = .pipe // ensure pipe protocol
            ble.sendFile(filename: "control.START.cmd", data: data)
            startFourBeatCountIn()
        }
    }

    @MainActor
    private func startFourBeatCountIn() {
        // Cancel any existing count-in by resetting state
        isCountingIn = true
        currentBeat = 0

        let interval = max(0.05, 60.0 / Double(tempoBPM))

        Task { @MainActor in
            for beat in 1...4 {
                currentBeat = beat
                try? await Task.sleep(nanoseconds: UInt64(interval * 1_000_000_000))
            }
            // End of count-in
            isCountingIn = false
            currentBeat = 0
        }
    }
}

#Preview {
    ContentView()
        .modelContainer(for: Item.self, inMemory: true)
}
