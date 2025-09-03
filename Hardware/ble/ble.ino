#include <NimBLEDevice.h>

// UUIDs (randomly generated, you can make your own at uuidgenerator.net)
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcdefab-1234-5678-1234-abcdefabcdef"

NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;

void setup() {
  Serial.begin(115200);

  // Initialize BLE
  NimBLEDevice::init("Wand1");  // name that shows on laptop/phone

  // Create BLE Server
  pServer = NimBLEDevice::createServer();

  // Create Service
  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  // Create Characteristic (read & notify)
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                    );

  // Set initial value
  pCharacteristic->setValue("Hello from ESP32!");

  // Start Service
  pService->start();

  // Start Advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  NimBLEDevice::startAdvertising();

  Serial.println("BLE server started, waiting for client connection...");
}

void loop() {
  static int counter = 0;
  delay(2000);

  // Update characteristic value every 2s
  char buffer[32];
  sprintf(buffer, "Counter: %d", counter++);
  pCharacteristic->setValue(buffer);
  pCharacteristic->notify();  // send update to connected client

  Serial.println(buffer);
}
