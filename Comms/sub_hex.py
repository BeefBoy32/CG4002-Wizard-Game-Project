# sub_hex.py
import struct, paho.mqtt.client as mqtt

BROKER = "172.20.10.5"   # your laptop IP
PORT   = 1884

def on_message(client, userdata, msg):
    b = msg.payload
    # pretty hex
    hexbytes = " ".join(f"{x:02X}" for x in b)
    print(f"{msg.topic}  [{len(b)}B]  {hexbytes}")

    if len(b) == 7:
        flags = b[0]
        ax, ay, az = struct.unpack("<hhh", b[1:7])
        drawing = (flags >> 1) & 1
        endbit  = flags & 1
        wand_id = (flags >> 2) & 1
        print(f"  flags=0b{flags:08b} drawing={drawing} wand_id={wand_id} end_bit={endbit} ax={ax} ay={ay} az={az}")

cli = mqtt.Client("hex-sub")
cli.connect(BROKER, PORT, 60)
cli.subscribe("wand/imu7", qos=0)
cli.on_message = on_message
print("listening...")
cli.loop_forever()
