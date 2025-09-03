#include <BLE.h>

// UUIDs (randomly generated, you can make your own at uuidgenerator.net)
#define SERVER_NAME         "Wand_1"
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcdefab-1234-5678-1234-abcdefabcdef"
BLE myBLE(SERVER_NAME, SERVICE_UUID, CHARACTERISTIC_UUID);

void setup() {
  Serial.begin(115200);
  myBLE.initialize_BLE();
}

void loop() {
  static int counter = 0;
  delay(2000);

  // Update characteristic value every 2s
  char buffer[32];
  sprintf(buffer, "Counter: %d", counter++);
  myBLE.send_update(buffer);
  Serial.println(buffer);
}
