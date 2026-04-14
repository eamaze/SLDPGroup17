#ifndef MIDI_PARSER_H
#define MIDI_PARSER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <vector>

// Structure to represent a MIDI note event
struct MidiNote {
    uint8_t noteNumber;      // MIDI note number (0-127)
    uint16_t onsetTime;      // Time in milliseconds (approximate)
    
    // Helper to get frequency from MIDI note number
    float getFrequency() const {
        // A4 (MIDI note 69) = 440 Hz
        // Formula: frequency = 440 * 2^((note - 69) / 12)
        return 440.0 * pow(2.0, (noteNumber - 69.0) / 12.0);
    }
};

// Structure to represent a note event with timing
struct NoteEvent {
    uint8_t highestNote;     // Highest MIDI note number at this event
    float targetFrequency;   // Frequency of the highest note
    uint16_t timeMs;         // When this note should be played
};

class MidiParser {
private:
    std::vector<NoteEvent> noteSequence;
    uint32_t fileSize;
    
    // Helper methods for MIDI parsing
    uint32_t readVariableLength(File& file);
    uint8_t readByte(File& file);
    uint16_t readBigEndian16(File& file);
    uint32_t readBigEndian32(File& file);
    
public:
    MidiParser();
    
    // Parse a MIDI file from SPIFFS
    bool parseMidiFile(const char* filename);
    
    // Get the sequence of notes to play
    const std::vector<NoteEvent>& getNoteSequence() const {
        return noteSequence;
    }
    
    // Get number of notes in the sequence
    uint16_t getNoteCount() const {
        return noteSequence.size();
    }
    
    // Clear the current sequence
    void clear() {
        noteSequence.clear();
    }
};

#endif
