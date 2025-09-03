#ifndef BLE_H   // include guard (prevents double inclusion)
#define BLE_H

#include <Arduino.h>
#include <NimBLEDevice.h>

class BLE {
  public:
    // Constructor
    BLE(const char* name, const char* service_UUID, const char* characteristic_UUID);
    void initialize_BLE();
    void send_update(char(&buffer) [32]);

  private:
    const char* _SERVER_NAME;
    const char* _SERVICE_UUID;
    const char* _CHARACTERISTIC_UUID;  
    NimBLEServer* _pServer;
    NimBLEService* _pService;
    NimBLECharacteristic* _pCharacteristic;
    NimBLEAdvertising* _pAdvertising;
};

#endif