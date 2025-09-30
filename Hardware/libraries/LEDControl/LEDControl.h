#ifndef LEDCONTROL_H   // include guard (prevents double inclusion)
#define LEDCONTROL_H

#include <Arduino.h>

enum Colour {
  RED,
  BLUE,
  GREEN,
  YELLOW,
  PURPLE,
  CYAN
}; 

const char* colourToString(Colour c);

class LEDControl {
  public:
    // Constructor
    LEDControl(int redPin, int bluePin, int greenPin, int frequency, int resolution);
    void initializeLED();
    void on_initialize_light();
    void off_light();
    void on_spell_light(Colour colour, int strength);

  private:
    int _colourPins[3];
    int _freq;
    int _res;
    static const int COL_DIV = 51; // 255/5
};

#endif