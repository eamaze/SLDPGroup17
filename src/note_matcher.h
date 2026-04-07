#pragma once
#include <Arduino.h>
#include <vector>
#include <math.h>

/**
 * NoteMatcher
 * ───────────
 * Holds a list of target notes (name + octave) and decides whether a
 * detected note matches any of them.
 *
 * Matching is enharmonic-aware: "C#" and "Db" are treated as the same pitch.
 *
 * The tolerance window is ±50 cents by default, meaning the detected pitch
 * just needs to be the *closest* semitone to one of the targets — not
 * perfectly in tune. You can tighten this with setTolerance().
 */
class NoteMatcher {
public:
  struct TargetNote {
    String name;
    int    octave;
    double freqHz;  // pre-computed for reference
  };

  NoteMatcher() : _toleranceCents(50.0) {}

  /** Add a note that should trigger the LED (e.g. "A", 4  →  A4 = 440 Hz). */
  void addTargetNote(const String& name, int octave) {
    TargetNote t;
    t.name   = name;
    t.octave = octave;
    t.freqHz = noteToFreq(name, octave);
    _targets.push_back(t);
  }

  /** Set matching tolerance in cents (100 cents = 1 semitone). Default: 50. */
  void setTolerance(double cents) { _toleranceCents = cents; }

  /**
   * Returns true if the detected note (name + octave) matches any target.
   * The names are compared after normalising enharmonics to sharps.
   */
  bool isMatch(const String& detectedName, int detectedOctave) {
    String normDetected = normalise(detectedName);
    for (auto& t : _targets) {
      if (normalise(t.name) == normDetected && t.octave == detectedOctave) {
        return true;
      }
    }
    return false;
  }

  /** Print all configured target notes to Serial. */
  void printTargets() {
    Serial.println("[TARGETS] Configured notes:");
    for (auto& t : _targets) {
      Serial.printf("  %s%d  (%.2f Hz)\n", t.name.c_str(), t.octave, t.freqHz);
    }
  }

  /** Reference frequency for any note name + octave. */
  static double noteToFreq(const String& name, int octave) {
    // Map note name to semitone index (0 = C)
    static const char* NAMES[] = {
      "C","C#","D","D#","E","F","F#","G","G#","A","A#","B",
      // enharmonic aliases:
      "Db","Eb","Gb","Ab","Bb"
    };
    static const int INDICES[] = {
      0,1,2,3,4,5,6,7,8,9,10,11,
      1,3,6,8,10
    };

    int semitone = -1;
    for (int i = 0; i < 17; i++) {
      if (name.equalsIgnoreCase(NAMES[i])) { semitone = INDICES[i]; break; }
    }
    if (semitone < 0) return 0.0;

    // Semitones above A4 (440 Hz)
    // A4 is octave 4, note index 9
    int semiAboveA4 = (octave - 4) * 12 + (semitone - 9);
    return 440.0 * pow(2.0, semiAboveA4 / 12.0);
  }

private:
  std::vector<TargetNote> _targets;
  double _toleranceCents;

  /** Normalise flat names to their sharp equivalents for comparison. */
  static String normalise(const String& name) {
    if (name == "Db" || name == "db") return "C#";
    if (name == "Eb" || name == "eb") return "D#";
    if (name == "Gb" || name == "gb") return "F#";
    if (name == "Ab" || name == "ab") return "G#";
    if (name == "Bb" || name == "bb") return "A#";
    return name;
  }
};
