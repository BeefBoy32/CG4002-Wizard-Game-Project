#include "Wire.h"
#include "MAX17043.h"
void setup() {
  Serial.begin(115200);
  Wire.begin();
  if (!FuelGauge.begin(&Wire)) {
    Serial.println("MAX17043 NOT found.\n");
    while (true) {}
  }
  FuelGauge.reset();
  delay(250);
  FuelGauge.quickstart();
  delay(125);
}

void loop() {
  float volts = FuelGauge.voltage();
  float pcnt = FuelGauge.percent();
  Serial.printf("%.0fmV (%.1f%%)\n", volts, pcnt);
  delay(3000);
}
/*
#include <Wire.h>

void setup() {
  Wire.begin(21, 22);
  Serial.begin(115200);

  Serial.println("Scanning I2C bus...");
  for (uint8_t i = 1; i < 127; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found I2C device at 0x");
      Serial.println(i, HEX);
    }
  }
}

void loop() {}
*/
