// ButtonManager.h
#pragma once
#include <Arduino.h>

class ButtonManager {
public:
    ButtonManager(int pin, unsigned long debounceDelay = 50);
    bool isPressed();
    void update();

private:
    int _pin;
    int _lastState;
    int _lastFlickerableState;
    unsigned long _lastDebounceTime;
    unsigned long _debounceDelay;
};