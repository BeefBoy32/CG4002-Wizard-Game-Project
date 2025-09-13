#include "LEDControl.h"

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
    ledcWrite(_colourPins[BLUE], 255);
}

void LEDControl::off_light() {
    ledcWrite(_colourPins[RED], 0);
    ledcWrite(_colourPins[BLUE], 0);
    ledcWrite(_colourPins[GREEN], 0);
}