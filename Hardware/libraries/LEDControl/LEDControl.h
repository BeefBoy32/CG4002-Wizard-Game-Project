#ifndef LEDCONTROL_H   // include guard (prevents double inclusion)
#define LEDCONTROL_H

#include <Arduino.h>

enum Channel {
  RED,
  BLUE,
  GREEN
}; 

class LEDControl {
  public:
    // Constructor
    LEDControl(int redPin, int bluePin, int greenPin, int frequency, int resolution);
    void initializeLED();
    void on_initialize_light();
    void off_light();
  private:
    int _colourPins[3];
    int _freq;
    int _res;
};

#endif