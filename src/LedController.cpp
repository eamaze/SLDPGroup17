#include "LedController.h"

LedController::LedController(const uint8_t wPins[12], const uint8_t rPins[12]) {
    for (int i = 0; i < 12; i++) {
        whitePins[i] = wPins[i];
        redPins[i] = rPins[i];
        missTimers[i] = 0;
    }
}

void LedController::begin() {
    for (int i = 0; i < 12; i++) {
        pinMode(whitePins[i], OUTPUT);
        pinMode(redPins[i], OUTPUT);
        digitalWrite(whitePins[i], LOW);
        digitalWrite(redPins[i], LOW);
    }
}

void LedController::setTargetNote(uint8_t midiNote, bool state) {
    uint8_t index = midiNote % 12; // Map 128 MIDI notes to 12 semitone LEDs
    digitalWrite(whitePins[index], state ? HIGH : LOW);
}

void LedController::triggerMiss(uint8_t midiNote) {
    uint8_t index = midiNote % 12;
    digitalWrite(redPins[index], HIGH);
    missTimers[index] = millis(); // Start the pulse timer
}

void LedController::update() {
    unsigned long currentMillis = millis();
    for (int i = 0; i < 12; i++) {
        if (missTimers[i] > 0 && (currentMillis - missTimers[i] >= PULSE_DURATION)) {
            digitalWrite(redPins[i], LOW); // Turn off red LED
            missTimers[i] = 0;             // Reset timer
        }
    }
}

void LedController::clearAll() {
    for (int i = 0; i < 12; i++) {
        digitalWrite(whitePins[i], LOW);
        digitalWrite(redPins[i], LOW);
        missTimers[i] = 0;
    }
}