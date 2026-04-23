#include "LedController.h"

LedController::LedController(const uint8_t tPins[12], uint8_t cPin, uint8_t mPin) {
    for (int i = 0; i < 12; i++) {
        targetPins[i] = tPins[i];
    }
    correctPin = cPin;
    missPin = mPin;
    
    correctTimer = 0;
    missTimer = 0;
    
    currentMode = MODE_IDLE_CHASE; 
    effectTimer = 0;
    effectStep = 0;
    flashCount = 0;
}

void LedController::begin() {
    for (int i = 0; i < 12; i++) {
        pinMode(targetPins[i], OUTPUT);
    }
    pinMode(correctPin, OUTPUT);
    pinMode(missPin, OUTPUT);
    clearAll(); 
}

void LedController::setEffectMode(LedEffectMode newMode) {
    if (currentMode == newMode) return;
    
    currentMode = newMode;
    effectTimer = millis();
    effectStep = 0;
    flashCount = 0;
    clearAll(); 
}

void LedController::setTargetNote(uint8_t midiNote, bool state) {
    if (currentMode != MODE_NORMAL) return; 
    uint8_t index = midiNote % 12; 
    digitalWrite(targetPins[index], state ? HIGH : LOW);
}

void LedController::triggerCorrect() {
    if (currentMode != MODE_NORMAL) return;
    digitalWrite(correctPin, HIGH);
    correctTimer = millis();
}

void LedController::triggerMiss() {
    if (currentMode != MODE_NORMAL) return;
    digitalWrite(missPin, HIGH);
    missTimer = millis();
}

void LedController::handleEffects() {
    unsigned long currentMillis = millis();

    switch (currentMode) {
        case MODE_IDLE_CHASE: {
            if (currentMillis - effectTimer > 100) { 
                clearAll();
                effectStep = (effectStep + 1) % 12;
                digitalWrite(targetPins[effectStep], HIGH);
                effectTimer = currentMillis;
            }
            break;
        }

        case MODE_LOAD_FLASH: {
            if (currentMillis - effectTimer < 500) {
                if (effectStep == 0) {
                    for (int i=0; i<12; i++) digitalWrite(targetPins[i], HIGH);
                    digitalWrite(correctPin, HIGH);
                    digitalWrite(missPin, HIGH);
                    effectStep = 1;
                }
            } else {
                clearAll();
                setEffectMode(MODE_NORMAL); 
            }
            break;
        }

        case MODE_END_FLASH: {
            unsigned long elapsed = currentMillis - effectTimer;
            if (flashCount < 3) {
                bool isOn = (elapsed % 600) < 300;
                
                if ((isOn && effectStep == 0) || (!isOn && effectStep == 1)) {
                    for (int i=0; i<12; i++) digitalWrite(targetPins[i], isOn ? HIGH : LOW);
                    digitalWrite(correctPin, isOn ? HIGH : LOW);
                    effectStep = isOn ? 1 : 0;
                    
                    if (!isOn && effectStep == 0) flashCount++; 
                }
            } else {
                clearAll();
                setEffectMode(MODE_NORMAL); 
            }
            break;
        }

        case MODE_NORMAL:
            break;
    }
}

void LedController::update() {
    unsigned long currentMillis = millis();

    if (currentMode != MODE_NORMAL) {
        handleEffects();
        return; 
    }

    // Normal Gameplay Timer Checks for the Correct/Miss LEDs
    if (correctTimer > 0 && (currentMillis - correctTimer >= PULSE_DURATION)) {
        digitalWrite(correctPin, LOW);
        correctTimer = 0;
    }
    
    if (missTimer > 0 && (currentMillis - missTimer >= PULSE_DURATION)) {
        digitalWrite(missPin, LOW);
        missTimer = 0;
    }
}

void LedController::clearAll() {
    for (int i = 0; i < 12; i++) {
        digitalWrite(targetPins[i], LOW);
    }
    digitalWrite(correctPin, LOW);
    digitalWrite(missPin, LOW);
}