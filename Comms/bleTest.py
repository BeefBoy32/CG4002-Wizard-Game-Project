import asyncio
from bleak import BleakClient, BleakScanner

ESP32_MAC = "3c:8a:1f:54:f1:d2"  # Replace with your ESP32 MAC

async def main():
    # Optional: scan for devices to confirm MAC exists
    devices = await BleakScanner.discover()
    for d in devices:
        print(d.address, d.name)

    # Connect by MAC
    async with BleakClient(ESP32_MAC) as client:
        print("Connected:", client.is_connected)
        # Example: read characteristic
        # value = await client.read_gatt_char("CHAR_UUID")
        # print(value)

asyncio.run(main())