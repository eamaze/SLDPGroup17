#include "SongManager.h"

SongManager::SongManager(LedController* ledController) 
    : leds(ledController), state(SONG_IDLE), startTime(0), 
      leniencyWindow(150), audioLatencyOffset(50), frequencyTolerance(25.0),
      hitCount(0), missCount(0) {
}

bool SongManager::loadSong(const char* filename, uint16_t bpm) {
    if (!parser.parseMidiFile(filename, bpm)) {
        Serial.println("[SONG] Failed to parse MIDI file.");
        state = SONG_IDLE;
        leds->setEffectMode(MODE_IDLE_GRADIENT); // Revert to gradient on failure
        return false;
    }
    
    if (parser.getNoteCount() == 0) {
        Serial.println("[SONG] No notes found in MIDI file");
        state = SONG_IDLE;
        leds->setEffectMode(MODE_IDLE_GRADIENT);
        return false;
    }
    
    currentSongFilename = String(filename);
    resetSong(); // Resets counters and states
    
    // --> Trigger the Load Flash animation! <--
    leds->setEffectMode(MODE_LOAD_FLASH); 
    
    Serial.printf("[SONG] Loaded %s with %d notes at %d BPM\n", filename, parser.getNoteCount(), bpm);
    return true;
}

void SongManager::startPlaying() {
    if (state == SONG_LOADED) {
        state = SONG_PLAYING;
        startTime = millis();
        leds->setEffectMode(MODE_NORMAL); // Ensure LEDs are ready for gameplay
        Serial.println("[SONG] Playhead STARTED!");
    }
}

void SongManager::updatePlayhead(float currentPitch) {
    if (state != SONG_PLAYING) return;
    
    // Current logical time in the song, accounting for processing latency
    long currentPlayTime = (long)(millis() - startTime) - (long)audioLatencyOffset;
    
    std::vector<NoteEvent>& sequence = parser.getMutableNoteSequence();
    bool allEvaluated = true;
    
    for (auto& note : sequence) {
        if (note.evaluated) continue;
        
        allEvaluated = false; // We found a note that still needs processing
        long timeDiff = currentPlayTime - (long)note.timeMs;
        
        // 1. Target Window Approaching: Turn on Green LED slightly early
        if (timeDiff >= -500 && timeDiff < -leniencyWindow) {
            leds->setTargetNote(note.highestNote, true);
        }
        
        // 2. Active Leniency Window (Hit Box)
        if (abs(timeDiff) <= leniencyWindow) {
            leds->setTargetNote(note.highestNote, true);
            
            // Check for hit
            if (currentPitch > 0 && abs(currentPitch - note.targetFrequency) <= frequencyTolerance) {
                note.hit = true;
                note.evaluated = true;
                hitCount++;
                
                Serial.printf("[HIT] Note %d / Freq %.1f\n", note.highestNote, note.targetFrequency);
                leds->setTargetNote(note.highestNote, false); // Turn off Green LED on hit
            }
        }
        
        // 3. Window Passed (Miss)
        else if (timeDiff > leniencyWindow) {
            note.evaluated = true;
            missCount++;
            
            Serial.printf("[MISS] Note %d\n", note.highestNote);
            leds->setTargetNote(note.highestNote, false); // Turn off Green LED
            leds->triggerMiss(note.highestNote);          // Pulse Blue LED
        }
        
        // Optimization: Notes are chronological.
        if (timeDiff < -500) break; 
    }
    
    // Check for song end
    if (allEvaluated) {
        state = SONG_FINISHED;
        
        // --> Trigger the 3x Green Flash animation! <--
        leds->setEffectMode(MODE_END_FLASH); 
        
        Serial.printf("[SONG] COMPLETED! Accuracy: %.1f%%\n", getAccuracy());
    }
}

float SongManager::getAccuracy() const {
    uint16_t total = parser.getNoteCount();
    if (total == 0) return 0.0;
    return ((float)hitCount / total) * 100.0;
}

void SongManager::resetSong() {
    std::vector<NoteEvent>& sequence = parser.getMutableNoteSequence();
    for (auto& note : sequence) {
        note.evaluated = false;
        note.hit = false;
    }
    hitCount = 0;
    missCount = 0;
    state = SONG_LOADED;
    // (Notice we removed leds->clearAll() here so the End Flash animation isn't interrupted!)
}

void SongManager::unloadSong() {
    parser.clear();
    state = SONG_IDLE;
    currentSongFilename = "";
    leds->setEffectMode(MODE_IDLE_GRADIENT); // Back to the idle gradient
}