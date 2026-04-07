#pragma once
#include <Arduino.h>
#include <math.h>

/**
 * PitchDetector
 * ─────────────
 * Finds the dominant frequency in an FFT magnitude spectrum and converts it
 * to a musical note name + octave using equal-temperament tuning (A4 = 440 Hz).
 *
 * Uses Harmonic Product Spectrum (HPS) to improve accuracy on voiced sounds
 * by reinforcing the fundamental even when a harmonic is loudest.
 */
class PitchDetector {
public:
  PitchDetector(uint32_t sampleRate, uint16_t fftSize, double threshold)
    : _sampleRate(sampleRate), _fftSize(fftSize), _threshold(threshold) {}

  /**
   * Detect the dominant pitch from an FFT magnitude buffer.
   * Returns the frequency in Hz, or 0.0 if no significant pitch found.
   */
  double detectPitch(double* magnitudes) {
    // ── Step 1: find peak magnitude to check signal level ─────────────────
    double peakMag = 0.0;
    for (int i = 1; i < _fftSize / 2; i++) {
      if (magnitudes[i] > peakMag) peakMag = magnitudes[i];
    }
    if (peakMag < _threshold) return 0.0;  // Too quiet — skip

    // ── Step 2: Harmonic Product Spectrum (HPS) ───────────────────────────
    // Multiply the spectrum by down-sampled copies of itself.
    // The fundamental frequency will align across harmonics and produce the
    // highest combined product, even if a harmonic is individually louder.
    const int HPS_HARMONICS = 4;
    int usableBins = (_fftSize / 2) / HPS_HARMONICS;

    double hps[usableBins];
    for (int i = 0; i < usableBins; i++) {
      hps[i] = magnitudes[i];
    }

    for (int h = 2; h <= HPS_HARMONICS; h++) {
      for (int i = 0; i < usableBins; i++) {
        hps[i] *= magnitudes[i * h];
      }
    }

    // ── Step 3: find the peak in the HPS ──────────────────────────────────
    // Skip the first few bins (< ~40 Hz) to avoid DC and sub-bass noise.
    int minBin = (int)(40.0 * _fftSize / _sampleRate);
    int maxBin = (int)(4200.0 * _fftSize / _sampleRate);  // ~C8
    maxBin = min(maxBin, usableBins - 1);

    int peakBin = minBin;
    double peakHPS = 0.0;
    for (int i = minBin; i <= maxBin; i++) {
      if (hps[i] > peakHPS) {
        peakHPS = hps[i];
        peakBin = i;
      }
    }

    // ── Step 4: parabolic interpolation for sub-bin accuracy ──────────────
    double freq = binToFreq(peakBin);
    if (peakBin > 0 && peakBin < usableBins - 1) {
      double alpha = hps[peakBin - 1];
      double beta  = hps[peakBin];
      double gamma = hps[peakBin + 1];
      double correction = 0.5 * (alpha - gamma) / (alpha - 2.0 * beta + gamma);
      freq = binToFreq((double)peakBin + correction);
    }

    return freq;
  }

  /**
   * Convert a frequency (Hz) to the nearest note name, octave, and cents deviation.
   * Uses equal temperament with A4 = 440 Hz reference.
   */
  void freqToNote(double freq, String& noteName, int& octave, double& centsDev) {
    static const char* NOTE_NAMES[] = {
      "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    // Number of semitones above C0 (C0 ≈ 16.35 Hz)
    // A4 is semitone 57 above C0 (9 semitones above C4, which is 4 octaves up)
    double semitones = 12.0 * log2(freq / 440.0) + 57.0;
    int    roundedSemitones = (int)round(semitones);

    centsDev  = (semitones - roundedSemitones) * 100.0;
    octave    = roundedSemitones / 12;
    int noteIndex = roundedSemitones % 12;
    if (noteIndex < 0) { noteIndex += 12; octave--; }

    noteName = NOTE_NAMES[noteIndex];
  }

private:
  uint32_t _sampleRate;
  uint16_t _fftSize;
  double   _threshold;

  inline double binToFreq(double bin) {
    return bin * (double)_sampleRate / (double)_fftSize;
  }
};
