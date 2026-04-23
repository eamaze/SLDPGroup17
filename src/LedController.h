#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>

enum LedEffectMode {
    MODE_NORMAL,         
    MODE_IDLE_CHASE,     
    MODE_LOAD_FLASH,     
    MODE_END_FLASH       
};

class LedController {
private:
    uint8_t targetPins[12];
    uint8_t correctPin;
    uint8_t missPin;
    
    unsigned long correctTimer;
    unsigned long missTimer;
    const unsigned long PULSE_DURATION = 300; // Pulse duration in ms

    // Animation tracking variables
    LedEffectMode currentMode;
    unsigned long effectTimer;
    int effectStep;
    int flashCount;

    // Internal handler called by update()
    void handleEffects();

public:
    LedController(const uint8_t tPins[12], uint8_t cPin, uint8_t mPin);
    
    void begin();
    
    void setTargetNote(uint8_t midiNote, bool state);
    void triggerCorrect();
    void triggerMiss();
    
    void setEffectMode(LedEffectMode newMode);

    void update();
    void clearAll();
};

#endif