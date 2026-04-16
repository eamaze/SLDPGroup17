//
//  MIDINoteEvent.swift
//  test
//
//  Created by Kevin L on 4/8/26.
//


import Foundation
import AudioToolbox

private let pitchClassNames = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]

private func noteName(from midiNote: UInt8) -> String {
    pitchClassNames[Int(midiNote) % 12]
}

public struct MIDINoteEvent: Hashable {
    public let startBeat: MusicTimeStamp
    public let durationBeats: Float32
    public let channel: UInt8
    public let note: String
    public let velocity: UInt8
}

public func parseMIDINotesOnly(from midiFileURL: URL) throws -> [MIDINoteEvent] {
    var seqOpt: MusicSequence?
    var status = NewMusicSequence(&seqOpt)
    guard status == noErr, let sequence = seqOpt else {
        throw NSError(domain: "MIDI", code: Int(status))
    }

    status = MusicSequenceFileLoad(sequence, midiFileURL as CFURL, .midiType, MusicSequenceLoadFlags())
    guard status == noErr else {
        throw NSError(domain: "MIDI", code: Int(status))
    }

    var trackCount: UInt32 = 0
    MusicSequenceGetTrackCount(sequence, &trackCount)

    var result: [MIDINoteEvent] = []

    for i in 0..<trackCount {
        var trackOpt: MusicTrack?
        MusicSequenceGetIndTrack(sequence, i, &trackOpt)
        guard let track = trackOpt else { continue }

        var iterOpt: MusicEventIterator?
        status = NewMusicEventIterator(track, &iterOpt)
        guard status == noErr, let iter = iterOpt else {
            throw NSError(domain: "MIDI", code: Int(status))
        }
        defer { DisposeMusicEventIterator(iter) }

        var hasEvent: DarwinBoolean = false
        MusicEventIteratorHasCurrentEvent(iter, &hasEvent)

        while hasEvent.boolValue {
            var timeStamp: MusicTimeStamp = 0
            var eventType: MusicEventType = 0
            var eventData: UnsafeRawPointer?
            var eventDataSize: UInt32 = 0

            MusicEventIteratorGetEventInfo(iter, &timeStamp, &eventType, &eventData, &eventDataSize)

            if eventType == kMusicEventType_MIDINoteMessage, let eventData {
                let msg = eventData.assumingMemoryBound(to: MIDINoteMessage.self).pointee

                result.append(
                    MIDINoteEvent(
                        startBeat: timeStamp,
                        durationBeats: msg.duration,
                        channel: msg.channel,
                        note: noteName(from: msg.note),   // CHANGED
                        velocity: msg.velocity
                    )
                )
            }

            MusicEventIteratorNextEvent(iter)
            MusicEventIteratorHasCurrentEvent(iter, &hasEvent)
        }
    }

    result.sort { $0.startBeat < $1.startBeat }
    return result
}
