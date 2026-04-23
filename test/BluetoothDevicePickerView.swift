//
//  BluetoothDevicePickerView.swift
//  test
//
//  Created by Kevin L on 4/16/26.
//


import SwiftUI
import CoreBluetooth

struct BluetoothDevicePickerView: View {
    @EnvironmentObject var ble: BLEManager
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        List {
            Section {
                HStack {
                    Text("Bluetooth")
                    Spacer()
                    Text(ble.isBluetoothOn ? "On" : "Off")
                        .foregroundStyle(ble.isBluetoothOn ? .green : .red)
                }
                HStack {
                    Text("Status")
                    Spacer()
                    Text(ble.status).foregroundStyle(.secondary)
                }
                if let name = ble.deviceName {
                    HStack {
                        Text("Connected")
                        Spacer()
                        Text(name).foregroundStyle(.secondary)
                    }
                }
            }

            Section("Discovered Devices") {
                if ble.discoveredDevices.isEmpty {
                    Text(ble.isScanning ? "Scanning…" : "No devices found")
                        .foregroundStyle(.secondary)
                }

                ForEach(ble.discoveredDevices, id: \.peripheral.identifier) { item in
                    Button {
                        ble.connect(to: item.peripheral)
                    } label: {
                        HStack {
                            Image(systemName: "antenna.radiowaves.left.and.right")
                            Text(item.name.isEmpty ? "Unknown" : item.name)
                        }
                    }
                }
            }
        }
        .navigationTitle("Bluetooth Devices")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                if ble.isScanning {
                    Button("Stop") { ble.stopScan() }
                } else {
                    Button("Scan") { ble.startScan() }
                        .disabled(!ble.isBluetoothOn || ble.isConnected)
                }
            }
            ToolbarItem(placement: .bottomBar) {
                HStack {
                    Button("Disconnect") { ble.disconnect() }
                        .disabled(!ble.isConnected)
                    Spacer()
                    Button("Done") { dismiss() }
                }
            }
        }
        .onAppear {
            // Convenient: start scanning automatically if not connected
            if ble.isBluetoothOn, !ble.isConnected, !ble.isScanning {
                ble.startScan()
            }
        }
    }
}