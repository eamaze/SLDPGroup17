#include "SongManager.h"

SongManager::SongManager() 
    : currentNoteIndex(0), currentTargetFreq(440.0), currentTargetNote(69), 
      state(SONG_IDLE), noteHitTime(0), noteJustHit(false) {
}

bool SongManager::loadSong(const char* filename) {
    // Try to parse the MIDI file
    if (!parser.parseMidiFile(filename)) {
        Serial.println("[SONG] Failed to parse MIDI file: " + String(filename));
        state = SONG_IDLE;
        return false;
    }
    
    // Get the parsed note sequence
    noteSequence = parser.getNoteSequence();
    
    if (noteSequence.empty()) {
        Serial.println("[SONG] No notes found in MIDI file");
        state = SONG_IDLE;
        return false;
    }
    
    // Set up to start from the beginning
    currentNoteIndex = 0;
    currentTargetNote = noteSequence[0].highestNote;
    currentTargetFreq = noteSequence[0].targetFrequency;
    currentSongFilename = String(filename);
    state = SONG_LOADED;
    noteJustHit = false;
    
    Serial.print("[SONG] Loaded song: ");
    Serial.print(filename);
    Serial.print(" with ");
    Serial.print(noteSequence.size());
    Serial.println(" notes");
    
    Serial.print("[SONG] First note: MIDI ");
    Serial.print(currentTargetNote);
    Serial.print(" (");
    Serial.print(currentTargetFreq, 1);
    Serial.println(" Hz)");
    
    return true;
}

bool SongManager::noteHit() {
    if (state == SONG_IDLE || noteSequence.empty()) {
        return false;
    }
    
    Serial.print("[SONG] Note hit! Index ");
    Serial.print(currentNoteIndex);
    Serial.print(" of ");
    Serial.print(noteSequence.size());
    Serial.println();
    
    // Mark when note was hit for LED confirmation
    noteHitTime = millis();
    noteJustHit = true;
    state = SONG_PLAYING;
    
    // Check if this is the last note
    if (isLastNote()) {
        Serial.println("[SONG] *** SONG COMPLETED! ***");
        state = SONG_FINISHED;
        return false;  // No more notes
    }
    
    // Move to next note
    currentNoteIndex++;
    if (currentNoteIndex < noteSequence.size()) {
        currentTargetNote = noteSequence[currentNoteIndex].highestNote;
        currentTargetFreq = noteSequence[currentNoteIndex].targetFrequency;
        
        Serial.print("[SONG] Next note: MIDI ");
        Serial.print(currentTargetNote);
        Serial.print(" (");
        Serial.print(currentTargetFreq, 1);
        Serial.print(" Hz) at index ");
        Serial.println(currentNoteIndex);
        
        return true;  // More notes to play
    }
    
    return false;  // No more notes
}

bool SongManager::shouldShowConfirmation() const {
    if (!noteJustHit) {
        return false;
    }
    
    unsigned long elapsed = millis() - noteHitTime;
    return elapsed < CONFIRMATION_DURATION;
}

void SongManager::restartSong() {
    if (noteSequence.empty()) {
        return;
    }
    
    currentNoteIndex = 0;
    currentTargetNote = noteSequence[0].highestNote;
    currentTargetFreq = noteSequence[0].targetFrequency;
    state = SONG_LOADED;
    noteJustHit = false;
    
    Serial.print("[SONG] Restarted song: ");
    Serial.println(currentSongFilename);
    Serial.print("[SONG] First note: MIDI ");
    Serial.print(currentTargetNote);
    Serial.print(" (");
    Serial.print(currentTargetFreq, 1);
    Serial.println(" Hz)");
}

void SongManager::newSongReceived(const char* filename) {
    state = SONG_NEW_RECEIVED;
    Serial.print("[SONG] New song received: ");
    Serial.println(filename);
    
    // Automatically load the new song
    if (!loadSong(filename)) {
        Serial.println("[SONG] Failed to load new song");
        state = SONG_IDLE;
    } else {
        Serial.println("[SONG] New song loaded and ready to play!");
    }
}

void SongManager::unloadSong() {
    noteSequence.clear();
    currentNoteIndex = 0;
    currentTargetFreq = 440.0;
    currentTargetNote = 69;
    state = SONG_IDLE;
    noteJustHit = false;
    currentSongFilename = "";
    
    Serial.println("[SONG] Song unloaded");
}
