import asyncio
from bleak import BleakClient, BleakScanner

ESP32_MAC = "3c:8a:1f:54:f1:d2"  # Replace with your ESP32 MAC
CHARACTERISTIC_UUID = "abcdefab-1234-5678-1234-abcdefabcdef"

def parse_vector(data: bytes):
    """Parse a 7-byte BLE packet into header info and VectorInt16"""
    if len(data) != 7:
        print("Invalid packet length:", len(data))
        return None

    header = data[0]
    device_id = header & 0x0F       # bits 0-3
    end_flag  = (header & 0x80) != 0  # bit 7

    # Little-endian 16-bit integers
    x = int.from_bytes(data[1:3], byteorder='little', signed=True)
    y = int.from_bytes(data[3:5], byteorder='little', signed=True)
    z = int.from_bytes(data[5:7], byteorder='little', signed=True)

    return {
        "device_id": device_id,
        "end_flag": end_flag,
        "x": x,
        "y": y,
        "z": z
    }

def notification_handler(sender, data):
    """Callback called when a notification is received"""
    vector = parse_vector(data)
    if vector:
        print(f"Received from device {vector['device_id']}: "
              f"x={vector['x']}, y={vector['y']}, z={vector['z']}, end={vector['end_flag']}")

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
        await client.start_notify(CHARACTERISTIC_UUID, notification_handler)

        print("Listening for BLE notifications... Press Ctrl+C to exit.")
        while True:
            await asyncio.sleep(1)

    

asyncio.run(main())
