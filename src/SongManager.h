#ifndef SONG_MANAGER_H
#define SONG_MANAGER_H

#include "MidiParser.h"
#include <Arduino.h>

enum SongState {
    SONG_IDLE,           // No song loaded
    SONG_LOADED,         // Song loaded, waiting for first note
    SONG_PLAYING,        // In the middle of playing
    SONG_FINISHED,       // Song completed
    SONG_NEW_RECEIVED    // New song received - needs restart
};

class SongManager {
private:
    MidiParser parser;
    std::vector<NoteEvent> noteSequence;
    
    uint16_t currentNoteIndex;  // Current position in the song
    float currentTargetFreq;    // Current target frequency
    uint8_t currentTargetNote;  // Current MIDI note number
    
    SongState state;
    String currentSongFilename;
    
    unsigned long noteHitTime;  // When the note was successfully hit
    bool noteJustHit;           // Flag to indicate note was just detected
    
    const unsigned long CONFIRMATION_DURATION = 500;  // LED flash duration in ms
    
public:
    SongManager();
    
    // Load a song from SPIFFS
    bool loadSong(const char* filename);
    
    // Get the current target frequency
    float getCurrentTargetFrequency() const {
        return currentTargetFreq;
    }
    
    // Get the current target MIDI note
    uint8_t getCurrentTargetNote() const {
        return currentTargetNote;
    }
    
    // Get the current note index
    uint16_t getCurrentNoteIndex() const {
        return currentNoteIndex;
    }
    
    // Check if we're on the last note
    bool isLastNote() const {
        return currentNoteIndex >= noteSequence.size() - 1;
    }
    
    // Mark that the current note was hit and move to next
    // Returns true if there are more notes, false if song is finished
    bool noteHit();
    
    // Check if the confirmation LED should still be on
    bool shouldShowConfirmation() const;
    
    // Get the current state
    SongState getState() const {
        return state;
    }
    
    // Restart the current song from the beginning
    void restartSong();
    
    // Signal that a new song was received over Bluetooth
    void newSongReceived(const char* filename);
    
    // Get the total number of notes in the song
    uint16_t getTotalNotes() const {
        return noteSequence.size();
    }
    
    // Get the current song filename
    String getCurrentSongName() const {
        return currentSongFilename;
    }
    
    // Unload current song
    void unloadSong();
    
    // Check if song is loaded
    bool isSongLoaded() const {
        return noteSequence.size() > 0;
    }
};

#endif
