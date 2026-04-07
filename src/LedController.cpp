#include "LedController.h"

LedController::LedController(uint8_t ledPin) {
    pin = ledPin;
    isOn = false;
}

void LedController::begin() {
    pinMode(pin, OUTPUT);
    turnOff();
}

void LedController::turnOn() {
    digitalWrite(pin, HIGH);
    isOn = true;
}

void LedController::turnOff() {
    digitalWrite(pin, LOW);
    isOn = false;
}

void LedController::setState(bool state) {
    if (state) {
        turnOn();
    } else {
        turnOff();
    }
}

bool LedController::getState() {
    return isOn;
}