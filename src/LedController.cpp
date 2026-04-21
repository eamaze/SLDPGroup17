#include "LedController.h"
#include <math.h> // Needed for sine wave gradient

LedController::LedController(Adafruit_TLC5947 *tlcController)
{
    tlc = tlcController;
    for (int i = 0; i < 12; i++)
    {
        missTimers[i] = 0;
        incorrectTimers[i] = 0;
    }
    currentMode = MODE_IDLE_GRADIENT; // Start in idle mode
    effectTimer = 0;
    effectStep = 0;
    flashCount = 0;
}

void LedController::begin()
{
    clearAll();
}

void LedController::setLedState(uint8_t channel, bool state)
{
    // 0 = ON (Full), 4095 = OFF
    tlc->setPWM(channel, state ? 0 : 4095);
}

void LedController::setLedPWM(uint8_t channel, uint16_t brightness)
{
    // brightness: 0 (Off) to 4095 (Max brightness)
    // Invert for JZ-MOS + Pull-up logic
    uint16_t pwmValue = 4095 - constrain(brightness, 0, 4095);
    tlc->setPWM(channel, pwmValue);
}

void LedController::setEffectMode(LedEffectMode newMode)
{
    if (currentMode == newMode)
        return;

    currentMode = newMode;
    effectTimer = millis();
    effectStep = 0;
    flashCount = 0;
    clearAll(); // Reset lights on state change
}

void LedController::setTargetNote(uint8_t midiNote, bool state)
{
    if (currentMode != MODE_NORMAL)
        return; // Only process notes in normal mode
    uint8_t index = midiNote % 12;
    setLedState(GREEN_OFFSET + index, state);
    tlc->write();
}

void LedController::triggerMiss(uint8_t midiNote)
{
    if (currentMode != MODE_NORMAL)
        return;
    uint8_t index = midiNote % 12;
    setLedState(BLUE_OFFSET + index, true);
    missTimers[index] = millis();
    tlc->write();
}

void LedController::triggerIncorrect(uint8_t midiNote)
{
    if (currentMode != MODE_NORMAL)
        return;
    uint8_t index = midiNote % 12;
    setLedState(RED_OFFSET + index, true);
    incorrectTimers[index] = millis();
    tlc->write();
}
void LedController::handleEffects()
{
    unsigned long currentMillis = millis();

    switch (currentMode)
    {

        // ... [Keep your existing cases here] ...

    case MODE_IDLE_GRADIENT:
    {
        // Creates a moving wave across all 36 channels
        for (int i = 0; i < 36; i++)
        {
            // Calculate sine wave: time factor + offset for each LED
            float wave = sin((currentMillis / 300.0) + (i * 0.3));
            uint16_t brightness = (wave + 1.0) * 2047.5; // Map -1/1 to 0-4095
            setLedPWM(i, brightness);
        }
        tlc->write();
        break;
    }

    case MODE_LOAD_FLASH:
    {
        // Flash all lights ON for 500ms, then OFF, then switch to NORMAL
        if (currentMillis - effectTimer < 500)
        {
            if (effectStep == 0)
            {
                for (int i = 0; i < 36; i++)
                    setLedState(i, true);
                tlc->write();
                effectStep = 1;
            }
        }
        else
        {
            clearAll();
            setEffectMode(MODE_NORMAL); // Transition to gameplay mode
        }
        break;
    }

    case MODE_END_FLASH:
    {
        // Flash Green LEDs 3 times (ON for 300ms, OFF for 300ms)
        unsigned long elapsed = currentMillis - effectTimer;
        if (flashCount < 3)
        {
            bool isOn = (elapsed % 600) < 300;

            // Only update if state changes
            if ((isOn && effectStep == 0) || (!isOn && effectStep == 1))
            {
                for (int i = 0; i < 12; i++)
                    setLedState(GREEN_OFFSET + i, isOn);
                tlc->write();
                effectStep = isOn ? 1 : 0;

                if (!isOn && effectStep == 0)
                    flashCount++; // Count completed flash
            }
        }
        else
        {
            clearAll();
            setEffectMode(MODE_IDLE_GRADIENT); // Go back to idle
        }
        break;
    }

    case MODE_TEST_ROWS:
    {
        // Cycle every 1000ms (1 second). Modulo 4 gives us steps 0, 1, 2, 3
        unsigned long elapsed = currentMillis - effectTimer;
        int currentStep = (elapsed / 1000) % 4;

        // Only update the TLC chip if the step has changed
        if (effectStep != currentStep || elapsed < 10)
        {

            // Manually clear states without calling write() yet to prevent flickering
            for (int i = 0; i < 36; i++)
            {
                setLedState(i, false);
            }

            // Turn on the specific row based on the current step
            if (currentStep == 0)
            {
                for (int i = 0; i < 12; i++)
                    setLedState(GREEN_OFFSET + i, true);
            }
            else if (currentStep == 1)
            {
                for (int i = 0; i < 12; i++)
                    setLedState(RED_OFFSET + i, true);
            }
            else if (currentStep == 2)
            {
                for (int i = 0; i < 12; i++)
                    setLedState(BLUE_OFFSET + i, true);
            }
            // currentStep == 3 leaves all LEDs off for a 1-second breather

            tlc->write();
            effectStep = currentStep;
        }
        break;
    }

    case MODE_NORMAL:
        // Standard game logic (handled in update())
        break;
    }
}

void LedController::update()
{
    unsigned long currentMillis = millis();
    bool needsUpdate = false;

    if (currentMode != MODE_NORMAL)
    {
        handleEffects();
        return; // Skip normal miss/incorrect timers if animating
    }

    // Normal Gameplay Timer Checks
    for (int i = 0; i < 12; i++)
    {
        if (missTimers[i] > 0 && (currentMillis - missTimers[i] >= PULSE_DURATION))
        {
            setLedState(BLUE_OFFSET + i, false);
            missTimers[i] = 0;
            needsUpdate = true;
        }
        if (incorrectTimers[i] > 0 && (currentMillis - incorrectTimers[i] >= PULSE_DURATION))
        {
            setLedState(RED_OFFSET + i, false);
            incorrectTimers[i] = 0;
            needsUpdate = true;
        }
    }

    if (needsUpdate)
    {
        tlc->write();
    }
}

void LedController::clearAll()
{
    for (int i = 0; i < 36; i++)
    {
        setLedState(i, false);
    }
    tlc->write();
}