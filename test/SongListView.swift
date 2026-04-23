import SwiftUI

struct SongListView: View {
    @EnvironmentObject var session: SessionManager
    @EnvironmentObject var profiles: ProfileStore
    @EnvironmentObject var ble: BLEManager

    private var username: String { session.activeUsername ?? "" }

    var body: some View {
        List {
            Section("Songs") {
                ForEach(SongLibrary.songs) { song in
                    let best = profiles.completionValue(username: username, songID: song.id) ?? 0

                    NavigationLink {
                        SongDetailView(song: song)
                    } label: {
                        VStack(alignment: .leading, spacing: 8) {
                            Text(song.name)
                                .font(.headline)

                            BestCompletionBar(value: best)
                        }
                        .padding(.vertical, 2)
                    }
                }
            }

            Section("Bluetooth") {
                NavigationLink {
                    BluetoothDevicePickerView()
                } label: {
                    HStack {
                        Text("Select Device")
                        Spacer()
                        if ble.isConnected {
                            Text(ble.deviceName ?? "Connected")
                                .foregroundStyle(.secondary)
                        } else {
                            Text("Not connected")
                                .foregroundStyle(.secondary)
                        }
                    }
                }
            }

            Section {
                Button("Sign Out", role: .destructive) {
                    session.signOut()
                }
            }
        }
        .navigationTitle("Welcome, \(username)")
    }
}

private struct BestCompletionBar: View {
    /// Expected 0...100
    let value: Int

    private var clamped: Double {
        let v = max(0, min(100, value))
        return Double(v) / 100.0
    }

    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .leading) {
                // Track
                RoundedRectangle(cornerRadius: 999)
                    .frame(height: 4)

                // Fill
                RoundedRectangle(cornerRadius: 999)
                    .frame(width: max(0, geo.size.width * clamped), height: 4)
            }
        }
        .frame(height: 4)
    }
}
