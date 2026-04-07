# ESP32 Pitch Detector with LED Feedback

Listens to a microphone, detects the musical pitch being played, and lights an LED whenever a pre-configured note is heard.

---

## Hardware Required

| Component | Notes |
|---|---|
| ESP32 dev board | Any standard 30- or 38-pin module |
| INMP441 I²S MEMS mic | Or any I²S microphone (SPH0645, ICS-43434…) |
| LED | 3 mm or 5 mm, any colour |
| 330 Ω resistor | In series with the LED |
| Breadboard + jumpers | |

---

## Wiring

```
INMP441 Microphone           ESP32
─────────────────────────────────────
VDD  ──────────────────────► 3.3V
GND  ──────────────────────► GND
SD   ──────────────────────► GPIO 32  (I2S_DATA_IN)
SCK  ──────────────────────► GPIO 14  (I2S_BCLK)
WS   ──────────────────────► GPIO 15  (I2S_LRCLK)
L/R  ──────────────────────► GND  (selects LEFT channel)

LED
─────────────────────────────────────
Anode (+) ─► 330Ω ─────────► GPIO 2
Cathode(-)──────────────────► GND
```

> **Tip:** GPIO 2 is the onboard LED on most ESP32 dev boards, so you may not even need an external LED during development.

---

## Software Setup (PlatformIO)

1. Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO extension](https://platformio.org/install/ide?install=vscode).
2. Open this folder as a PlatformIO project.
3. PlatformIO will automatically download the `arduinoFFT` library declared in `platformio.ini`.
4. Click **Upload** (→) to flash the firmware.
5. Open **Serial Monitor** at **115200 baud** to watch live output.

---

## How It Works

```
┌─────────────┐    I²S bus     ┌──────────────┐   FFT magnitudes   ┌──────────────────┐
│ INMP441 mic │ ─────────────► │  ESP32 I²S   │ ─────────────────► │  arduinoFFT      │
│  (analog    │                │  peripheral  │                    │  (Hann window +  │
│   MEMS)     │                │              │                    │   HPS algorithm) │
└─────────────┘                └──────────────┘                    └────────┬─────────┘
                                                                            │
                                                                     dominant freq (Hz)
                                                                            │
                                                                   ┌────────▼─────────┐
                                                                   │  PitchDetector   │
                                                                   │  freqToNote()    │
                                                                   │  → "A", octave 4 │
                                                                   └────────┬─────────┘
                                                                            │
                                                                   ┌────────▼─────────┐
                                                                   │  NoteMatcher     │
                                                                   │  isMatch()?      │
                                                                   └────────┬─────────┘
                                                                            │
                                                                       YES  │  NO
                                                                      ┌─────▼──────┐
                                                                      │  LED ON/OFF│
                                                                      └────────────┘
```

### Key algorithms

**Harmonic Product Spectrum (HPS)**
The FFT spectrum is multiplied by down-sampled copies of itself. The fundamental frequency reinforces itself across all harmonics and produces the highest product, even if an overtone is louder — making this robust for real instruments.

**Parabolic interpolation**
After finding the peak FFT bin, the neighbouring bin magnitudes are used to fit a parabola and find the true sub-bin peak position, giving sub-Hz frequency resolution without a larger FFT.

**Equal-temperament conversion**
Frequency → note uses `semitones = 12 × log₂(f / 440) + 57`, mapping to the closest semitone on the chromatic scale.

---

## Configuration

### Changing the target note(s)

In `src/main.cpp`, find the setup block and edit:

```cpp
noteMatcher.addTargetNote("A", 4);   // A4 = 440 Hz
noteMatcher.addTargetNote("E", 4);   // E4 ≈ 329.63 Hz
```

Call `addTargetNote()` as many times as you like. The LED will light up when **any** target note is matched.

### Tuning the sensitivity

| Parameter | Location | Effect |
|---|---|---|
| `AMPLITUDE_THRESHOLD` | `main.cpp` | Raise to ignore more background noise |
| `HPS_HARMONICS` | `pitch_detector.h` | 3–5 is ideal; higher = better low-note detection |
| `setTolerance(cents)` | `main.cpp` | Default 50¢ (half a semitone); tighten for strict matching |
| `FFT_SIZE` | `main.cpp` | Larger (4096) = better freq resolution but slower |

### Supported note names

`C  C#  D  D#  E  F  F#  G  G#  A  A#  B`

Flat aliases (`Db Eb Gb Ab Bb`) are also accepted and automatically normalised.

---

## Serial Output Example

```
=== ESP32 Pitch Detector ===
[I2S] Initialized OK
[TARGETS] Configured notes:
  A4  (440.00 Hz)
  E4  (329.63 Hz)
[READY] Listening for notes...

[PITCH] 440.23 Hz  →  A4  (0.9 cents off)
  ★ MATCH! LED ON  (A4)
[PITCH] 329.81 Hz  →  E4  (0.9 cents off)
  ★ MATCH! LED ON  (E4)
[PITCH] 261.90 Hz  →  C4  (-0.7 cents off)
```

---

## Extending the Project

| Idea | How |
|---|---|
| Multiple LEDs per note | Add more `LED_PIN_x` defines; `NoteMatcher` can return *which* target matched |
| NeoPixel colour per note | Replace `digitalWrite` with `strip.setPixelColor(0, noteColour)` |
| Note name on OLED | Add `Adafruit_SSD1306` library and render `noteName + octave` |
| Chord detection | Run multiple HPS passes or compare top-N peaks against all target notes |
| Save target notes to flash | Use `Preferences` library (ESP32 NVS) |

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| No serial output | Wrong baud rate | Set monitor to 115200 |
| Always silent (no pitch detected) | Threshold too high | Lower `AMPLITUDE_THRESHOLD` |
| Wildly wrong notes | Wrong I²S pins | Double-check wiring against pin table |
| Correct note but LED never lights | Note name/octave mismatch | Check `addTargetNote()` arguments |
| Noisy / jumping detections | Mic too close to speaker | Add distance; try raising threshold |
