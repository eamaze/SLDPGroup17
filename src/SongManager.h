#ifndef SONG_MANAGER_H
#define SONG_MANAGER_H

#include "MidiParser.h"
#include "LedController.h"
#include <Arduino.h>

enum SongState {
    SONG_IDLE,           
    SONG_LOADED,         // Loaded, waiting for BT "START" command
    SONG_PLAYING,        // Playhead is active
    SONG_FINISHED        
};

class SongManager {
private:
    MidiParser parser;
    LedController* leds; // Reference to LED controller
    
    SongState state;
    String currentSongFilename;
    
    unsigned long startTime;
    uint32_t leniencyWindow;      // +/- ms window for a hit
    uint32_t audioLatencyOffset;  // fixed ms offset for processing delay
    float frequencyTolerance;
    
    // Accuracy tracking
    uint16_t hitCount;
    uint16_t missCount;
    
public:
    SongManager(LedController* ledController);
    
    // Configuration setters
    void setLeniencyWindow(uint32_t ms) { leniencyWindow = ms; }
    void setAudioLatencyOffset(uint32_t ms) { audioLatencyOffset = ms; }
    void setFrequencyTolerance(float hz) { frequencyTolerance = hz; }
    
    // Load file with provided BPM
    bool loadSong(const char* filename, uint16_t bpm);
    
    // Start playhead
    void startPlaying();
    
    // Main continuous playhead logic. Pass the current detected pitch (or -1.0 if none)
    void updatePlayhead(float currentPitch);
    
    float getAccuracy() const;
    SongState getState() const { return state; }
    bool isSongLoaded() const { return state != SONG_IDLE; }
    uint16_t getTotalNotes() const { return parser.getNoteCount(); }
    
    void resetSong();
    void unloadSong();
};

#endif