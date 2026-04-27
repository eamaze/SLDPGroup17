#include "MidiParser.h"
#include <algorithm> // <--- ADD THIS FOR std::sort
MidiParser::MidiParser() : fileSize(0) {
}

bool MidiParser::parseMidiFile(const char* filename, uint16_t bpm) {
    clear();
    
    File midiFile = SPIFFS.open(filename, "r");
    if (!midiFile) {
        Serial.println("[MIDI] Failed to open file: " + String(filename));
        return false;
    }
    
    fileSize = midiFile.size();
    Serial.print("[MIDI] Parsing file: ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(fileSize);
    Serial.println(" bytes)");
    
    // Read MIDI header
    char header[4];
    if (midiFile.readBytes(header, 4) != 4 || 
        header[0] != 'M' || header[1] != 'T' || 
        header[2] != 'h' || header[3] != 'd') {
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
    
    // Skip remaining header bytes
    if (headerLen > 6) {
        midiFile.seek(midiFile.position() + (headerLen - 6));
    }
    
    Serial.print("[MIDI] Format: ");
    Serial.print(format);
    Serial.print(", Tracks: ");
    Serial.print(numTracks);
    Serial.print(", Division (ticks/beat): ");
    Serial.println(division);
    
    // Calculate ms per beat dynamically based on provided BPM
    // 60,000 ms per minute / BPM = ms per beat
    // Calculate ms per beat dynamically based on provided BPM
    // Fallback to prevent divide-by-zero crashes if Bluetooth sends 0
    uint16_t safeBpm = (bpm > 0) ? bpm : 120; 
    uint32_t msPerBeat = 60000UL / safeBpm;
    
    // Parse each track
    uint32_t currentTime = 0;  // In milliseconds
    
    for (uint16_t trackIdx = 0; trackIdx < numTracks; trackIdx++) {
        // Read track header
        char trackHeader[4];
        if (midiFile.readBytes(trackHeader, 4) != 4 || 
            trackHeader[0] != 'M' || trackHeader[1] != 'T' || 
            trackHeader[2] != 'r' || trackHeader[3] != 'k') {
            Serial.println("[MIDI] Invalid track header");
            midiFile.close();
            return false;
        }
        
        uint32_t trackLen = readBigEndian32(midiFile);
        uint32_t trackStart = midiFile.position();
        
        Serial.print("[MIDI] Track ");
        Serial.print(trackIdx);
        Serial.print(" length: ");
        Serial.println(trackLen);
        
        uint32_t trackTime = 0;  // Time within this track
        uint8_t lastStatus = 0;  // Running status for MIDI events
        
        // Process track events
        while (midiFile.position() < trackStart + trackLen) {
            // Read variable length delta time
            uint32_t deltaTime = readVariableLength(midiFile);
            trackTime += deltaTime;
            
            // New absolute time calculation using dynamic BPM
            // Cast to uint64_t prevents 32-bit integer overflow during multiplication
            uint32_t timeMs = (uint32_t)(((uint64_t)trackTime * msPerBeat) / division);            
            
            uint8_t status = readByte(midiFile);
            
            // Handle running status (reuse previous status if high bit not set)
            if (!(status & 0x80)) {
                midiFile.seek(midiFile.position() - 1);  // Back up
                status = lastStatus;
            } else {
                // CRITICAL FIX: Only Voice and Mode messages update running status.
                // System messages (0xF0 to 0xFF) MUST NOT overwrite it.
                if (status < 0xF0) {
                    lastStatus = status;
                }
            }
            
            uint8_t eventType = status & 0xF0;
            uint8_t channel = status & 0x0F;
            
            if (eventType == 0x90) {  // Note On
                uint8_t noteNumber = readByte(midiFile);
                uint8_t velocity = readByte(midiFile);
                
                if (velocity > 0) {  // velocity 0 is typically treated as Note Off
                    Serial.print("[MIDI] Note On: ");
                    Serial.print(noteNumber);
                    Serial.print(" at ");
                    Serial.print(timeMs);
                    Serial.println(" ms");
                    
                    // Note event structured for playhead tracking
                    NoteEvent event;
                    event.highestNote = noteNumber;
                    event.targetFrequency = MidiNote{noteNumber, (uint16_t)timeMs}.getFrequency();
                    event.timeMs = timeMs;
                    event.evaluated = false;
                    event.hit = false;
                    noteSequence.push_back(event);
                }
            } else if (eventType == 0x80) {  // Note Off
                readByte(midiFile);  // Note number
                readByte(midiFile);  // Velocity
            } else if (eventType == 0xB0) {  // Control Change
                readByte(midiFile);  // Controller
                readByte(midiFile);  // Value
            } else if (eventType == 0xC0) {  // Program Change
                readByte(midiFile);  // Program
            } else if (eventType == 0xE0) {  // Pitch Bend
                readByte(midiFile);  // LSB
                readByte(midiFile);  // MSB
            } else if (eventType == 0xA0) {  // Aftertouch
                readByte(midiFile);  // Note number
                readByte(midiFile);  // Pressure
            } else if (eventType == 0xD0) {  // Channel Pressure
                readByte(midiFile);  // Pressure
            } else if (status == 0xFF) {  // Meta event
                uint8_t metaType = readByte(midiFile);
                uint32_t metaLen = readVariableLength(midiFile);
                
                if (metaType == 0x51) {  // Set Tempo
                    if (metaLen >= 3) {
                        uint8_t b1 = readByte(midiFile);
                        uint8_t b2 = readByte(midiFile);
                        uint8_t b3 = readByte(midiFile);
                        uint32_t tempo = (b1 << 16) | (b2 << 8) | b3;
                        Serial.print("[MIDI] Tempo: ");
                        Serial.print(tempo);
                        Serial.println(" microseconds per beat");
                    } else {
                        for (uint32_t i = 0; i < metaLen; i++) readByte(midiFile);
                    }
                } else if (metaType == 0x2F) {  // End of Track
                    for (uint32_t i = 0; i < metaLen; i++) readByte(midiFile);
                    break;
                } else {
                    // Skip other meta events
                    for (uint32_t i = 0; i < metaLen; i++) readByte(midiFile);
                }
            } else if (status == 0xF0 || status == 0xF7) {  // SysEx
                uint32_t sysexLen = readVariableLength(midiFile);
                for (uint32_t i = 0; i < sysexLen; i++) readByte(midiFile);
            }
        }
    }
        
    midiFile.close();
    
    Serial.print("[MIDI] Parsed ");
    Serial.print(noteSequence.size());
    Serial.println(" note events");
    
    // --- NEW: SORT THE SEQUENCE CHRONOLOGICALLY ---
    std::sort(noteSequence.begin(), noteSequence.end(), [](const NoteEvent& a, const NoteEvent& b) {
        return a.timeMs < b.timeMs;
    });

    // Simple post-processing: keep only note on events and consolidate simultaneous notes
    std::vector<NoteEvent> consolidated;
    
    for (size_t i = 0; i < noteSequence.size(); i++) {
        // Find all notes that happen at the same time
        uint32_t currentTime = noteSequence[i].timeMs; // <--- CHANGED TO uint32_t
        uint8_t highestNote = noteSequence[i].highestNote;
        
        // Look ahead for notes at same time
        while (i + 1 < noteSequence.size() && noteSequence[i + 1].timeMs == currentTime) {
            i++;
            if (noteSequence[i].highestNote > highestNote) {
                highestNote = noteSequence[i].highestNote;
            }
        }
        
        // Add consolidated event with highest note and tracking statuses
        NoteEvent event;
        event.highestNote = highestNote;
        event.targetFrequency = MidiNote{highestNote, currentTime}.getFrequency(); // Match type
        event.timeMs = currentTime;
        event.evaluated = false;
        event.hit = false;
        consolidated.push_back(event);
    }
    
    noteSequence = consolidated;
    
    Serial.print("[MIDI] Consolidated to ");
    Serial.print(noteSequence.size());
    Serial.println(" unique note events");
    
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
    if (!file.available()) {
        return 0;
    }
    return file.read();
}

uint16_t MidiParser::readBigEndian16(File& file) {
    uint8_t b1 = readByte(file);
    uint8_t b2 = readByte(file);
    return (b1 << 8) | b2;
}

uint32_t MidiParser::readBigEndian32(File& file) {
    uint8_t b1 = readByte(file);
    uint8_t b2 = readByte(file);
    uint8_t b3 = readByte(file);
    uint8_t b4 = readByte(file);
    return (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
}