#include "BLE.h"

BLE::BLE(const char* name, const char* service_UUID, const char* characteristic_UUID) {
    _SERVER_NAME = name;
    _SERVICE_UUID = service_UUID;
    _CHARACTERISTIC_UUID = characteristic_UUID;  
}

void BLE::initialize_BLE() {
    // Initialize BLE
    NimBLEDevice::init(_SERVER_NAME);  // name that shows on laptop/phone

    // Create BLE Server
    _pServer = NimBLEDevice::createServer();

    // Create Service
    _pService = _pServer->createService(_SERVICE_UUID);

    // Create Characteristic (read & notify)
    _pCharacteristic = _pService->createCharacteristic(
                        _CHARACTERISTIC_UUID,
                        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                    );

    // Set initial value
    _pCharacteristic->setValue("Hello from Wand 1!");

    // Start Service
    _pService->start();

    // Start Advertising
    _pAdvertising = NimBLEDevice::getAdvertising();
    _pAdvertising->addServiceUUID(_SERVICE_UUID);
    NimBLEDevice::startAdvertising();

    Serial.println("BLE server started, waiting for client connection...");
}

void BLE::send_update(char(&buffer) [32]) {
    _pCharacteristic->setValue(buffer);
    _pCharacteristic->notify();  // send update to connected client
}