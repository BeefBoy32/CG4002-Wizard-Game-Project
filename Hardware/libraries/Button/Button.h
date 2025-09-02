#ifndef BUTTON_H   // include guard (prevents double inclusion)
#define BUTTON_H

#include <Arduino.h>

class Button {
  public:
    // Constructor
    Button(int pin, int pressedTime);
    bool CheckHold();
    bool CheckRelease();
    bool IsInitialHold();
    bool IsInitialRelease();
    void InitializeButton();

  private:
    int _pin;
    int _holdTime;
    int _releaseTime;
    bool _initialHold;
    bool _initialRelease;
    bool _hold;
    bool _release;
    unsigned long _pressedTime;

};

#endif