#include "ButtonManager.h"

ButtonManager::ButtonManager(int pin, unsigned long debounceDelay)
    : _pin(pin), _lastState(HIGH), _lastFlickerableState(HIGH), _lastDebounceTime(0), _debounceDelay(debounceDelay), _stateChanged(false) {
    pinMode(_pin, INPUT_PULLUP);
}

bool ButtonManager::isPressed() {
    return _lastState == LOW;
}

void ButtonManager::update() {
    int currentState = digitalRead(_pin);

    if (currentState != _lastFlickerableState) {
        _lastDebounceTime = millis();
        _lastFlickerableState = currentState;
    }

    if ((millis() - _lastDebounceTime) > _debounceDelay) {
        if (_lastState != currentState) {
            _lastState = currentState;
            _stateChanged = true;
        }
    }
}

bool ButtonManager::stateChanged() {
    if (_stateChanged) {
        _stateChanged = false;
        return true;
    }
    return false;
}