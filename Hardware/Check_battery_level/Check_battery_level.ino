#define BAT_ADC 35   // Battery sense pin (A0 on FireBeetle ESP32)

void setup() {
  Serial.begin(115200);
}

void loop() {
  int raw = analogRead(BAT_ADC);

  // ESP32 ADC is 12-bit (0–4095), reference ~3.3 V
  float measuredV = (raw / 4095.0) * 3.3;

  // FireBeetle divides battery voltage in half before ADC
  float battV = measuredV * 2;

  Serial.print("Battery Voltage: ");
  Serial.print(battV);
  Serial.println(" V");

  if (battV < 3.3) {
    Serial.println("⚠️ Battery low! Going to deep sleep...");
    esp_deep_sleep_start();
  }

  delay(2000);
}
