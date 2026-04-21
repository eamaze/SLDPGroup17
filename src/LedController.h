#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_TLC5947.h>

enum LedEffectMode {
    MODE_NORMAL,         
    MODE_IDLE_GRADIENT,  
    MODE_LOAD_FLASH,     
    MODE_END_FLASH,
    MODE_TEST_ROWS       // <-- ADD THIS NEW MODE
};

class LedController {
private:
    Adafruit_TLC5947* tlc;
    
    unsigned long missTimers[12];
    unsigned long incorrectTimers[12];
    const unsigned long PULSE_DURATION = 300; // Pulse duration in ms

    // Animation tracking variables
    LedEffectMode currentMode;
    unsigned long effectTimer;
    int effectStep;
    int flashCount;

    // Channel offsets
    const uint8_t GREEN_OFFSET = 0;   // Channels 0-11
    const uint8_t RED_OFFSET = 12;    // Channels 12-23
    const uint8_t BLUE_OFFSET = 24;   // Channels 24-35

    // Helper for simple ON/OFF with inverted logic
    void setLedState(uint8_t channel, bool state);
    
    // Helper for fading/gradients with inverted logic (0-4095)
    void setLedPWM(uint8_t channel, uint16_t brightness);

    // Internal handler called by update()
    void handleEffects();

public:
    LedController(Adafruit_TLC5947* tlcController);
    
    void begin();
    
    // Target note controls
    void setTargetNote(uint8_t midiNote, bool state);
    void triggerMiss(uint8_t midiNote);
    void triggerIncorrect(uint8_t midiNote);
    
    // Change the current animation state
    void setEffectMode(LedEffectMode newMode);

    void update();
    void clearAll();
};

#endif