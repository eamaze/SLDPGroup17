import Foundation

struct Song: Identifiable, Hashable {
    /// Use the song name as the ID (must be unique).
    let id: String

    let name: String

    let tempoBPM: Int

    /// Derived MIDI filename in bundle / used for BLE transfer.
    var midiFilename: String { "\(name).mid" }
}

enum SongLibrary {
    static let songs: [Song] = [
        Song(id: "testMID", name: "testMID", tempoBPM: 120)
    ]
}
