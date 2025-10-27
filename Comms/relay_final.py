# from socket import socket
import json, time, collections, threading, socket, os
from bleak import cli
import paho.mqtt.client as mqtt
import ssl

# BROKER = "192.168.1.12"   # your laptop IP
# BROKER = "172.20.10.5"   # your laptop IP
BROKER = "localhost"  
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
T_WAND1_SPELL = "u96/wand1/spell"
T_WAND2_SPELL = "u96/wand2/spell"

SUBS = [
  ("wand1/mpu", 0), ("wand2/mpu", 0),
  ("wand1/cast",1), ("wand2/cast",1),
  ("wand1/batt",1), ("wand2/batt",1),
  ("wand1/status",1), ("wand2/status",1),
]

def listen_ultra96(cli: mqtt.Client):
    global sock
    leftover = b""
    while True:
        data = sock.recv(4096)
        if not data:
            print("Ultra96 connection closed"); break
        leftover += data
        while b"\n" in leftover:
            line, leftover = leftover.split(b"\n", 1)
            if not line.strip(): continue
            try:
                msg = json.loads(line.decode())
                wid = int(msg.get("wand_id", 0))
                cli.publish(T_WAND1_SPELL if wid == 1 else T_WAND2_SPELL,
                            json.dumps(msg), qos=2, retain=False)  # spell is critical
            except Exception as e:
                print("Error parsing Ultra96 message:", e)

def on_message(cli, _, msg):
    topic = msg.topic
    wid = 1 if topic.startswith("wand1") else 2
    if topic.endswith("/mpu"):
        payload = msg.payload.decode(errors="replace")
        try:
            d = json.loads(payload)
            if not isinstance(d, dict):
                raise ValueError("payload not a JSON object")
            line = {"wand_id": wid,
                    "mpu": {"yaw": d["yaw"], "pitch": d["pitch"], "roll": d["roll"],
                            "accelx": d["accelx"], "accely": d["accely"], "accelz": d["accelz"]}}
            sock.sendall((json.dumps(line) + "\n").encode())
        except Exception as e:
            print("Bad MPU JSON on", topic, "->", payload, "retain=", msg.retain, "err:", e)
            return

def main():
    global sock
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((ULTRA96_IP, ULTRA96_PORT))
    print(f"Connected to Ultra96 at {ULTRA96_IP}:{ULTRA96_PORT}")

    cli = mqtt.Client("relay-node")
    cli.on_message = on_message
    cli.connect(BROKER, PORT, 60)
    cli.subscribe(SUBS)

    # Tell the wand that U96 is alive & in drawing mode (retained)
    cli.publish(
        "u96/status",
        json.dumps({
            "ready": True,
            "wand1_state": {"drawingMode": True, "spell": "W"},  # any letter ok
            "wand2_state": {"drawingMode": True, "spell": "W"}
        }),
        qos=1, retain=True
    )

    threading.Thread(target=listen_ultra96, args=(cli,), daemon=True).start()
    print("Relay running…")
    cli.loop_forever()

if __name__ == "__main__":
    main()
