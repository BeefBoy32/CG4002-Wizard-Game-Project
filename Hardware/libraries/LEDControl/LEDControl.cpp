#include "LEDControl.h"

const char* colourToString(Colour c) {
  switch(c) {
    case RED:   return "RED";
    case GREEN: return "GREEN";
    case BLUE:  return "BLUE";
    case YELLOW:   return "YELLOW";
    case PURPLE: return "PURPLE";
    case CYAN:  return "CYAN";
    default:    return "WHITE";
  }
}

LEDControl::LEDControl(int redPin, int bluePin, int greenPin, int frequency, int resolution) {
    _colourPins[RED] = redPin;
    _colourPins[BLUE] = bluePin;
    _colourPins[GREEN] = greenPin;
    _freq = frequency;
    _res = resolution;
}

void LEDControl::initializeLED() {
    ledcAttachChannel(_colourPins[RED], _freq, _res, RED);
    ledcAttachChannel(_colourPins[BLUE], _freq, _res, BLUE);
    ledcAttachChannel(_colourPins[GREEN], _freq, _res, GREEN);
}

void LEDControl::on_initialize_light() {
    off_light();
    ledcWrite(_colourPins[GREEN], 150);
    ledcWrite(_colourPins[BLUE], 200);
    ledcWrite(_colourPins[RED], 140);
}

void LEDControl::off_light() {
    ledcWrite(_colourPins[RED], 0);
    ledcWrite(_colourPins[BLUE], 0);
    ledcWrite(_colourPins[GREEN], 0);
}

void LEDControl::on_spell_light(Colour colour, int strength) {
    switch (colour) {
        case RED:
            ledcWrite(_colourPins[RED], strength * COL_DIV);
            ledcWrite(_colourPins[BLUE], 0);
            ledcWrite(_colourPins[GREEN], 0);
            break;
        case GREEN:
            ledcWrite(_colourPins[GREEN], strength * COL_DIV);
            ledcWrite(_colourPins[BLUE], 0);
            ledcWrite(_colourPins[RED], 0);
            break;
        case BLUE:
            ledcWrite(_colourPins[BLUE], strength * COL_DIV);
            ledcWrite(_colourPins[GREEN], 0);
            ledcWrite(_colourPins[RED], 0);
            break;
        case YELLOW:
            ledcWrite(_colourPins[RED], strength * (int) (COL_DIV / 4.0) * 3);
            ledcWrite(_colourPins[GREEN], strength * COL_DIV);
            ledcWrite(_colourPins[BLUE], 0);
            break;
        case PURPLE:
            ledcWrite(_colourPins[RED], strength * (int) (COL_DIV / 2.0));
            ledcWrite(_colourPins[BLUE], strength * COL_DIV);
            ledcWrite(_colourPins[GREEN], 0);
            break;
        case CYAN:
            ledcWrite(_colourPins[GREEN], strength * (int) (COL_DIV / 2.0));
            ledcWrite(_colourPins[BLUE], strength * COL_DIV);
            ledcWrite(_colourPins[RED], 0);
            break;
        default:
            ledcWrite(_colourPins[GREEN], 128);
            ledcWrite(_colourPins[BLUE], 192);
            ledcWrite(_colourPins[RED], 96);
    }
}

