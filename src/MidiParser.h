#ifndef MIDI_PARSER_H
#define MIDI_PARSER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

struct MidiNote {
    uint8_t noteNumber;
    uint32_t onsetTime;

    float getFrequency() const {
        return 440.0f * powf(2.0f, (noteNumber - 69.0f) / 12.0f);
    }
};

struct NoteEvent {
    uint8_t highestNote;
    float targetFrequency;
    uint32_t timeMs;
    bool evaluated;
    bool hit;
};

class MidiParser {
private:
    std::vector<NoteEvent> noteSequence;
    uint32_t fileSize;

    uint32_t readVariableLength(File& file);
    uint8_t readByte(File& file);
    uint16_t readBigEndian16(File& file);
    uint32_t readBigEndian32(File& file);

public:
    MidiParser();

    bool parseMidiFile(const char* filename, uint16_t bpm);

    const std::vector<NoteEvent>& getNoteSequence() const { return noteSequence; }

    std::vector<NoteEvent>& getMutableNoteSequence() { return noteSequence; }

    uint16_t getNoteCount() const { return noteSequence.size(); }

    void clear() { noteSequence.clear(); }
};

#endif
