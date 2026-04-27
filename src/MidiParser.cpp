#include "MidiParser.h"
#include <algorithm>

MidiParser::MidiParser() : fileSize(0) {}

bool MidiParser::parseMidiFile(const char* filename, uint16_t bpm) {
    clear();

    File midiFile = LittleFS.open(filename, "r");
    if (!midiFile) {
        Serial.print("[MIDI] Failed to open file: ");
        Serial.println(filename);
        return false;
    }

    fileSize = midiFile.size();
    Serial.print("[MIDI] Parsing file: ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(fileSize);
    Serial.println(" bytes)");

    // Header
    char header[4];
    if (midiFile.readBytes(header, 4) != 4 || header[0] != 'M' || header[1] != 'T' || header[2] != 'h' || header[3] != 'd') {
        Serial.println("[MIDI] Invalid MIDI header");
        midiFile.close();
        return false;
    }

    uint32_t headerLen = readBigEndian32(midiFile);
    if (headerLen < 6) {
        Serial.println("[MIDI] Invalid header length");
        midiFile.close();
        return false;
    }

    uint16_t format = readBigEndian16(midiFile);
    uint16_t numTracks = readBigEndian16(midiFile);
    uint16_t division = readBigEndian16(midiFile);

    if (headerLen > 6) {
        midiFile.seek(midiFile.position() + (headerLen - 6));
    }

    Serial.print("[MIDI] Format: ");
    Serial.print(format);
    Serial.print(", Tracks: ");
    Serial.print(numTracks);
    Serial.print(", Division (ticks/beat): ");
    Serial.println(division);

    uint16_t safeBpm = (bpm > 0) ? bpm : 120;
    uint32_t msPerBeat = 60000UL / safeBpm;

    // Heuristic reserve to reduce reallocs
    noteSequence.reserve(256);

    for (uint16_t trackIdx = 0; trackIdx < numTracks; trackIdx++) {
        char trackHeader[4];
        if (midiFile.readBytes(trackHeader, 4) != 4 || trackHeader[0] != 'M' || trackHeader[1] != 'T' || trackHeader[2] != 'r' || trackHeader[3] != 'k') {
            Serial.println("[MIDI] Invalid track header");
            midiFile.close();
            return false;
        }

        uint32_t trackLen = readBigEndian32(midiFile);
        uint32_t trackStart = midiFile.position();

        uint32_t trackTime = 0;
        uint8_t lastStatus = 0;

        while (midiFile.position() < trackStart + trackLen) {
            uint32_t deltaTime = readVariableLength(midiFile);
            trackTime += deltaTime;

            uint32_t timeMs = (uint32_t)(((uint64_t)trackTime * msPerBeat) / division);

            uint8_t status = readByte(midiFile);

            if (!(status & 0x80)) {
                midiFile.seek(midiFile.position() - 1);
                status = lastStatus;
            } else {
                if (status < 0xF0) lastStatus = status;
            }

            uint8_t eventType = status & 0xF0;

            if (eventType == 0x90) {
                uint8_t noteNumber = readByte(midiFile);
                uint8_t velocity = readByte(midiFile);

                if (velocity > 0) {
                    NoteEvent ev;
                    ev.highestNote = noteNumber;
                    ev.targetFrequency = MidiNote{noteNumber, timeMs}.getFrequency();
                    ev.timeMs = timeMs;
                    ev.evaluated = false;
                    ev.hit = false;
                    noteSequence.push_back(ev);
                }
            } else if (eventType == 0x80) {
                readByte(midiFile);
                readByte(midiFile);
            } else if (eventType == 0xB0) {
                readByte(midiFile);
                readByte(midiFile);
            } else if (eventType == 0xC0) {
                readByte(midiFile);
            } else if (eventType == 0xE0) {
                readByte(midiFile);
                readByte(midiFile);
            } else if (eventType == 0xA0) {
                readByte(midiFile);
                readByte(midiFile);
            } else if (eventType == 0xD0) {
                readByte(midiFile);
            } else if (status == 0xFF) {
                uint8_t metaType = readByte(midiFile);
                uint32_t metaLen = readVariableLength(midiFile);

                // skip meta payload
                for (uint32_t i = 0; i < metaLen; i++) (void)readByte(midiFile);

                if (metaType == 0x2F) break;
            } else if (status == 0xF0 || status == 0xF7) {
                uint32_t sysexLen = readVariableLength(midiFile);
                for (uint32_t i = 0; i < sysexLen; i++) (void)readByte(midiFile);
            }
        }
    }

    midiFile.close();

    std::sort(noteSequence.begin(), noteSequence.end(), [](const NoteEvent& a, const NoteEvent& b) {
        return a.timeMs < b.timeMs;
    });

    // Consolidate in-place to reduce peak allocations
    size_t w = 0;
    for (size_t r = 0; r < noteSequence.size(); r++) {
        uint32_t t = noteSequence[r].timeMs;
        uint8_t highest = noteSequence[r].highestNote;

        while (r + 1 < noteSequence.size() && noteSequence[r + 1].timeMs == t) {
            r++;
            if (noteSequence[r].highestNote > highest) highest = noteSequence[r].highestNote;
        }

        NoteEvent ev;
        ev.highestNote = highest;
        ev.targetFrequency = MidiNote{highest, t}.getFrequency();
        ev.timeMs = t;
        ev.evaluated = false;
        ev.hit = false;

        noteSequence[w++] = ev;
    }
    noteSequence.resize(w);

    Serial.print("[MIDI] Note events: ");
    Serial.println(noteSequence.size());

    return true;
}

uint32_t MidiParser::readVariableLength(File& file) {
    uint32_t value = 0;
    uint8_t byte;

    do {
        byte = readByte(file);
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80);

    return value;
}

uint8_t MidiParser::readByte(File& file) {
    if (!file.available()) return 0;
    return (uint8_t)file.read();
}

uint16_t MidiParser::readBigEndian16(File& file) {
    uint8_t b1 = readByte(file);
    uint8_t b2 = readByte(file);
    return (uint16_t)((b1 << 8) | b2);
}

uint32_t MidiParser::readBigEndian32(File& file) {
    uint8_t b1 = readByte(file);
    uint8_t b2 = readByte(file);
    uint8_t b3 = readByte(file);
    uint8_t b4 = readByte(file);
    return ((uint32_t)b1 << 24) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 8) | (uint32_t)b4;
}
