#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>

class LedController {
private:
    uint8_t pin;
    bool isOn;

public:
    LedController(uint8_t ledPin);
    void begin();
    void turnOn();
    void turnOff();
    void setState(bool state);
    bool getState();
};

#endif