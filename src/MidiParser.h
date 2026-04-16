#ifndef MIDI_PARSER_H
#define MIDI_PARSER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <vector>

struct MidiNote {
    uint8_t noteNumber;      
    uint16_t onsetTime;      
    
    float getFrequency() const {
        return 440.0 * pow(2.0, (noteNumber - 69.0) / 12.0);
    }
};

struct NoteEvent {
    uint8_t highestNote;     
    float targetFrequency;   
    uint16_t timeMs;         
    bool evaluated;  // Tracks if the note window has passed
    bool hit;        // Tracks if the note was successfully played
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
    
    // BPM parameter added for dynamic timing calculation
    bool parseMidiFile(const char* filename, uint16_t bpm);
    
    const std::vector<NoteEvent>& getNoteSequence() const {
        return noteSequence;
    }
    
    std::vector<NoteEvent>& getMutableNoteSequence() {
        return noteSequence; // Allows SongManager to modify evaluated/hit states
    }
    
    uint16_t getNoteCount() const {
        return noteSequence.size();
    }
    
    void clear() {
        noteSequence.clear();
    }
};

#endif