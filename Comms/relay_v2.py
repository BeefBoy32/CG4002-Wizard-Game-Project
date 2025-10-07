# from socket import socket
import json, time, collections, threading, socket, os
from bleak import cli
import paho.mqtt.client as mqtt
import ssl

BROKER = "192.168.1.12"   # your laptop IP
PORT   = 1883            # your mosquitto port

# ULTRA96_IP = "172.26.191.147"  # update if needed
ULTRA96_IP = "127.0.0.1"  # update if needed
ULTRA96_PORT = 5000
sock = None

# Topics
# Subscribed topics
T_WAND_HELLO = "wand/hello"
T_WAND_MPU = "wand/mpu"
T_WAND_CAST = "wand/cast"
T_WAND_BATTERY = "wand/batt"

#Topics to publish to
T_WAND1_SPELL = "wand1/spell"
T_WAND2_SPELL = "wand2/spell"

def listen_ultra96(cli: mqtt.Client):
    global sock
    leftover = b""
    while True:
        data = sock.recv(4096)
        if not data:
            print("Ultra96 connection closed")
            break
        leftover += data
        while b"\n" in leftover:
            line, leftover = leftover.split(b"\n", 1)
            if not line.strip():
                continue
            try:
                msg = json.loads(line.decode())
                print("Ultra96 -> Laptop:", msg)

                # Expect: {"wand_id": 1/2, "spell_type":"C/W/T/S/Z/I", ...}
                wid    = int(msg.get("wand_id", 0))
                letter = str(msg.get("spell_type", "U"))[:1]

                cli.publish(T_WAND1_SPELL if wid == 1 else T_WAND2_SPELL, line, qos=1, retain=False)
                print(f"⬅️ U96 -> wand{wid}/spell: {letter}")
                
            except Exception as e:
                print("Error parsing Ultra96 message:", e)


# MQTT message handler
def on_message(cli, _, msg):
    global sock
    sock.sendall(msg.payload + b"\n")


def main():
    global sock
    # 1) Connect TCP once
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((ULTRA96_IP, ULTRA96_PORT))
    print(f"Connected to Ultra96 at {ULTRA96_IP}:{ULTRA96_PORT}")

    # 2) MQTT
    cli = mqtt.Client("relay-node")
    cli.connect(BROKER, PORT, 60)
    cli.on_message = on_message
    cli.subscribe([
        (T_WAND_HELLO, 1),
        (T_WAND_MPU, 0),
        (T_WAND_CAST, 1),
    ])

    # 3) Start listener thread for U96 -> relay
    threading.Thread(target=listen_ultra96, args=(cli,), daemon=True).start()
    print("Relay running…")
    cli.loop_forever()

if __name__ == "__main__":
    main()
