#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>

class LedController {
private:
    uint8_t whitePins[12];
    uint8_t redPins[12];
    
    unsigned long missTimers[12];
    const unsigned long PULSE_DURATION = 300; // Miss pulse duration in ms

public:
    // Pass arrays containing the GPIO pins for the 12 white and 12 red LEDs
    LedController(const uint8_t wPins[12], const uint8_t rPins[12]);
    
    void begin();
    
    // Turns the white LED on or off for the specific note target
    void setTargetNote(uint8_t midiNote, bool state);
    
    // Triggers the red LED to pulse for a missed note
    void triggerMiss(uint8_t midiNote);
    
    // Must be called in loop() to handle non-blocking red LED pulses
    void update();
    
    // Turns off all LEDs immediately
    void clearAll();
};

#endif