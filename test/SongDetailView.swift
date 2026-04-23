import SwiftUI

struct SongDetailView: View {
    @EnvironmentObject var session: SessionManager
    @EnvironmentObject var profiles: ProfileStore
    @EnvironmentObject var ble: BLEManager

    let song: Song

    @State private var lastCompletionValue: Int? = nil
    @State private var lastStatus: String? = nil
    @State private var isSending: Bool = false

    private var username: String { session.activeUsername ?? "" }

    var body: some View {
        ScrollView {
            VStack(spacing: 16) {
                header
                infoCards
                activityPanel
                startButton
            }
            .padding()
        }
        .navigationBarTitle("Song", displayMode: .inline)
        .onAppear {
            if lastCompletionValue == nil {
                lastCompletionValue = profiles.completionValue(username: username, songID: song.id)
            }
        }
        .onChange(of: ble.incomingLines.count) { _, _ in
            guard let line = ble.incomingLines.last else { return }
            handleIncomingLine(line)
        }
        .onChange(of: lastStatus) { _, newValue in
            if let newValue {
                print("[SongDetailView] lastStatus changed -> \(newValue)")
            } else {
                print("[SongDetailView] lastStatus cleared (nil)")
            }
        }
        .toolbar {
            ToolbarItem(placement: .navigationBarTrailing) {
                NavigationLink("Bluetooth") {
                    BluetoothDevicePickerView()
                }
            }
        }
    }

    private var header: some View {
        VStack(spacing: 10) {
            Image(systemName: "music.note.list")
                .font(.system(size: 46, weight: .bold))

            Text(song.name)
                .font(.title)
                .fontWeight(.bold)

            Text(song.midiFilename)
                .font(.caption)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 8)
    }

    private var infoCards: some View {
        VStack(spacing: 12) {
            HStack(spacing: 12) {
                StatCard(title: "Tempo", value: "\(song.tempoBPM)", subtitle: "BPM", icon: "metronome")

                StatCard(
                    title: "Bluetooth",
                    value: ble.isConnected ? "Connected" : "Not Connected",
                    subtitle: ble.deviceName ?? "—",
                    icon: "antenna.radiowaves.left.and.right"
                )
            }

            HStack(spacing: 12) {
                CompletionCard(completion: lastCompletionValue)
            }
        }
    }

    private var activityPanel: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 10) {
                Image(systemName: isSending ? "paperplane" : "wave.3.right")
                    .font(.system(size: 18, weight: .semibold))

                Text("Activity")
                    .font(.headline)

                Spacer()

                if isSending {
                    ProgressView().scaleEffect(0.9)
                }
            }

            if let lastStatus {
                Text(lastStatus).font(.subheadline)
            } else {
                Text("Tap Start to send the song to the device.")
                    .font(.subheadline)
            }

            Divider()

            let recent = Array(ble.incomingLines.suffix(6)).reversed()
            if recent.isEmpty {
                Text("No device messages yet.")
                    .font(.caption)
            } else {
                VStack(alignment: .leading, spacing: 6) {
                    ForEach(Array(recent.enumerated()), id: \.offset) { _, line in
                        Text(line)
                            .font(.system(size: 12, weight: .regular, design: .monospaced))
                            .lineLimit(1)
                    }
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(14)
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .strokeBorder(lineWidth: 1)
        )
    }

    private var startButton: some View {
        Button(action: startSongFlow) {
            HStack(spacing: 10) {
                Image(systemName: "play.fill")
                    .font(.system(size: 16, weight: .bold))
                Text(isSending ? "Sending…" : "Start")
                    .font(.system(size: 17, weight: .bold))
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 14)
        }
        .buttonStyle(.borderedProminent)
        .disabled(!ble.isConnected || isSending)
    }

    private func startSongFlow() {
        lastStatus = nil

        guard ble.isConnected else {
            lastStatus = "Not connected to BLE device."
            return
        }

        let filename = song.midiFilename

        guard let url = Bundle.main.url(forResource: song.name, withExtension: "mid") else {
            lastStatus = "Missing bundled file: \(filename)"
            return
        }

        do {
            isSending = true
            let data = try Data(contentsOf: url)

            // Send BPM and file immediately
            ble.sendCommandLine("BPM:\(song.tempoBPM)")
            ble.sendFile(filename: filename, data: data)

            lastStatus = "Sent \(filename) + BPM. Will send START in 4s…"

            // Delay the START command by 4 seconds to ensure device is ready
            DispatchQueue.main.asyncAfter(deadline: .now() + 4) {
                ble.sendCommandLine("BEGINSONG")
                lastStatus = "BEGINSONG sent. Waiting for completion…"
                isSending = false
            }
        } catch {
            lastStatus = "Failed to read file: \(error.localizedDescription)"
            isSending = false
        }
    }

    private func handleIncomingLine(_ line: String) {
        let trimmed = line.trimmingCharacters(in: .whitespacesAndNewlines)

        // NEW: parse "Song Complete! Accuracy: 83.5%"
        if let percent = parseAccuracyPercent(from: trimmed) {
            let value = Int(round(percent)) // keep your existing Int completion storage
            lastCompletionValue = value
            profiles.setCompletionValue(username: username, songID: song.id, value: value)
            lastStatus = "Completion updated: \(value)"
            return
        }

        // existing fallback: "SONG_COMPLETED|N" / "SONG_COMPLETED:N" / "SONG_COMPLETED"
        if trimmed.hasPrefix("SONG_COMPLETED") {
            let value = parseCompletionValue(from: trimmed) ?? 1
            lastCompletionValue = value
            profiles.setCompletionValue(username: username, songID: song.id, value: value)
            lastStatus = "Completion updated: \(value)"
        }
    }

    private func parseAccuracyPercent(from line: String) -> Double? {
        // Expected example: "Song Complete! Accuracy: 83.5%"
        // Grab the substring between "Accuracy:" and "%"
        guard let range = line.range(of: "Accuracy:") else { return nil }

        let after = line[range.upperBound...]
        guard let percentIdx = after.firstIndex(of: "%") else { return nil }

        let numberPart = after[..<percentIdx]
            .replacingOccurrences(of: " ", with: "")
            .replacingOccurrences(of: "\t", with: "")

        return Double(numberPart)
    }

    private func parseCompletionValue(from message: String) -> Int? {
        if let pipeIdx = message.firstIndex(of: "|") {
            let v = message[message.index(after: pipeIdx)...]
            return Int(v)
        }
        if let colonIdx = message.firstIndex(of: ":") {
            let v = message[message.index(after: colonIdx)...]
            return Int(v)
        }
        return nil
    }
}

private struct StatCard: View {
    let title: String
    let value: String
    let subtitle: String
    let icon: String

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 8) {
                Image(systemName: icon)
                    .font(.system(size: 16, weight: .semibold))
                Text(title)
                    .font(.caption)
                    .fontWeight(.semibold)
                Spacer()
            }

            Text(value)
                .font(.system(size: 22, weight: .bold))

            Text(subtitle)
                .font(.caption)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(14)
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .strokeBorder(lineWidth: 1)
        )
    }
}

private struct CompletionCard: View {
    let completion: Int?

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 8) {
                Image(systemName: "checkmark.seal")
                    .font(.system(size: 16, weight: .semibold))
                Text("Completion")
                    .font(.caption)
                    .fontWeight(.semibold)
                Spacer()
            }

            Text(completion.map(String.init) ?? "--")
                .font(.system(size: 28, weight: .bold))

            Text("Latest result")
                .font(.caption)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(14)
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .strokeBorder(lineWidth: 1)
        )
    }
}
