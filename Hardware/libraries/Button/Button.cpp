#include "Button.h"

// Constructor definition
Button::Button(int pin, int holdTime) {
  _pin = pin;   // store pin number
  _holdTime = holdTime;
  _initialHold = false;
  _pressedTime = 0;
  _hold = true;
}

bool Button::CheckHold() {
    _hold = _initialHold;

    int buttonState = digitalRead(_pin);

    if (buttonState == LOW) {  // Button is pressed
        if (_pressedTime == 0) {
            _pressedTime = millis();  // Record the time button was first pressed
        }

        // Check if the button has been held longer than threshold
        if ((millis() - _pressedTime) >= _holdTime) {
            _initialHold = true;
        }

    } else {
        // Button released; reset timer
        _pressedTime = 0;
        _initialHold = false;
        _hold = false;
    }
    
    return _hold;
}

bool Button::CheckRelease() {
    _release = _initialRelease;

    int buttonState = digitalRead(_pin);

    if (buttonState == HIGH) {  // Button is pressed
        if (_releaseTime == 0) {
            _releaseTime = millis();  // Record the time button was first pressed
        }

        // Check if the button has been held longer than threshold
        if ((millis() - _releaseTime) >= _holdTime) {
            _initialRelease = true;
        }

    } else {
        // Button released; reset timer
        _releaseTime = 0;
        _initialRelease = false;
        _release = false;
    }
    
    return _release;
}

bool Button::IsInitialHold() {
    return _initialHold && !_hold;
}

bool Button::IsInitialRelease() {
    return _initialRelease && !_release;
}

void Button::InitializeButton() {
    pinMode(_pin, INPUT_PULLUP);
}