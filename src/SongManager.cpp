#include "SongManager.h"

SongManager::SongManager(LedController *ledController)
    : leds(ledController), state(SONG_IDLE), startTime(0),
      leniencyWindow(150), audioLatencyOffset(50), frequencyTolerance(25.0),
      hitCount(0), missCount(0)
{
}

bool SongManager::loadSong(const char *filename, uint16_t bpm)
{
    if (!parser.parseMidiFile(filename, bpm))
    {
        Serial.println("[SONG] Failed to parse MIDI file.");
        state = SONG_IDLE;
        leds->setEffectMode(MODE_IDLE_CHASE); // Revert to chase on failure
        return false;
    }

    if (parser.getNoteCount() == 0)
    {
        Serial.println("[SONG] No notes found in MIDI file");
        state = SONG_IDLE;
        leds->setEffectMode(MODE_IDLE_CHASE);
        return false;
    }

    currentSongFilename = String(filename);
    resetSong(); // Resets counters and states

    leds->setEffectMode(MODE_LOAD_FLASH);

    Serial.printf("[SONG] Loaded %s with %d notes at %d BPM\n", filename, parser.getNoteCount(), bpm);
    return true;
}

void SongManager::startPlaying()
{
    if (state == SONG_LOADED)
    {
        state = SONG_PLAYING;
        startTime = millis();
        leds->setEffectMode(MODE_NORMAL);
        Serial.println("[SONG] Playhead STARTED!");
    }
}

void SongManager::updatePlayhead(float currentPitch)
{
    if (state != SONG_PLAYING)
        return;

    long currentPlayTime = (long)(millis() - startTime) - (long)audioLatencyOffset;

    std::vector<NoteEvent> &sequence = parser.getMutableNoteSequence();
    bool allEvaluated = true;

    for (auto &note : sequence)
    {
        if (note.evaluated)
            continue;

        allEvaluated = false;
        long timeDiff = currentPlayTime - (long)note.timeMs;
        long window = (long)leniencyWindow; // <--- ADD THIS EXPLICIT CAST

        // Use the signed 'window' variable for all comparisons
        if (timeDiff >= -500 && timeDiff < -window)
        {
            leds->setTargetNote(note.highestNote, true);
        }

        if (abs(timeDiff) <= window)
        {
            leds->setTargetNote(note.highestNote, true);

            if (currentPitch > 0 && abs(currentPitch - note.targetFrequency) <= frequencyTolerance)
            {
                note.hit = true;
                note.evaluated = true;
                hitCount++;

                Serial.printf("[HIT] Note %d / Freq %.1f\n", note.highestNote, note.targetFrequency);
                leds->setTargetNote(note.highestNote, false);
            }
        }
        else if (timeDiff > window)
        {
            note.evaluated = true;
            missCount++;

            Serial.printf("[MISS] Note %d\n", note.highestNote);
            leds->setTargetNote(note.highestNote, false);
            leds->triggerMiss(); 
        }

        if (timeDiff < -500)
            break;
    }

    if (allEvaluated)
    {
        state = SONG_FINISHED;
        leds->setEffectMode(MODE_END_FLASH);
        Serial.printf("[SONG] COMPLETED! Accuracy: %.1f%%\n", getAccuracy());
    }
}

float SongManager::getAccuracy() const
{
    uint16_t total = parser.getNoteCount();
    if (total == 0)
        return 0.0;
    return ((float)hitCount / total) * 100.0;
}

void SongManager::resetSong()
{
    std::vector<NoteEvent> &sequence = parser.getMutableNoteSequence();
    for (auto &note : sequence)
    {
        note.evaluated = false;
        note.hit = false;
    }
    hitCount = 0;
    missCount = 0;
    state = SONG_LOADED;
}

void SongManager::unloadSong()
{
    parser.clear();
    state = SONG_IDLE;
    currentSongFilename = "";
    leds->setEffectMode(MODE_IDLE_CHASE);
}