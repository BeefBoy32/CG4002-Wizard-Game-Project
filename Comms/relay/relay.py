# from socket import socket
import json, time, collections, threading, socket
from bleak import cli
import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion
import ssl

BROKER = "172.20.10.5"   # your laptop IP
PORT   = 1883            # your mosquitto port

ULTRA96_IP = "172.26.191.147"  # update if needed
ULTRA96_PORT = 5000
sock = None

# Topics
T_WAND_MPU = "wand/mpu"
T_WAND_CMD  = "wand/cmd"
T_WAND_CAST = "wand/cast"
T_U96_DRAW_IN  = "u96/draw/in"
T_U96_DRAW_OUT = "u96/draw/out"
T_U96_CAST_IN  = "u96/cast/in"

# TLS certs for Mosquitto
CA_CERT   = "../../certs/ca.crt"
CERT_FILE = "../../certs/server.crt"
KEY_FILE  = "../../certs/server.key"

# Buffer ~2s at 10Hz (adjust as you like)
WINDOW_LEN = 20

# one buffer per wand
buf = {0: collections.deque(maxlen=WINDOW_LEN),
       1: collections.deque(maxlen=WINDOW_LEN)}

# Takes the 7-byte message from ESP32 IMU and converts the raw ax/ay/az to g's
def mpu_json_to_point(b: bytes):
    d = json.loads(b.decode())
    f = d.get("flags", {})
    return {
        "ts":   int(d.get("ts", 0)),
        "ax":   float(d.get("accelx", 0.0)),  # g
        "ay":   float(d.get("accely", 0.0)),
        "az":   float(d.get("accelz", 0.0)),
        "yaw":  float(d.get("yaw", 0.0)),              # deg
        "pitch":float(d.get("pitch", 0.0)),
        "roll": float(d.get("roll", 0.0)),
        "wand_id": int(d.get("wand_id", 0)),
        # "drawing": bool(f.get("drawing", True)),
        # "end":     bool(f.get("end", False)),
    }

def listen_ultra96(cli: mqtt.Client):
    """Line-buffered listener: one JSON per line from Ultra96."""
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
                # Relay classification back to ESP32 if needed
                cli.publish(T_U96_DRAW_OUT, json.dumps(msg), qos=1)
                cmd = {
                    "cmd": "spell",
                    "spell_id": int(msg.get("spell_id", 0)),
                    "conf": float(msg.get("conf", 1.0)),
                    "flags": {"armed": True, "stable": bool(msg.get("stable", False))}
                }
                cli.publish(T_WAND_CMD, json.dumps(cmd), qos=1)
                print("⬅️ Relayed U96 result back to ESP32:", cmd)
            except Exception as e:
                print("Error parsing Ultra96 message:", e)


# MQTT message handler
def on_message(cli, _, msg):
    global sock
    if msg.topic == T_WAND_MPU:
        pt = mpu_json_to_point(msg.payload)
        wid = pt["wand_id"]
        
        if wid not in buf:
            buf[wid] = collections.deque(maxlen=WINDOW_LEN)
        buf[wid].append(pt)

        full = (len(buf[wid]) == WINDOW_LEN)

        if full:
            ts = [p["ts"] for p in buf[wid] if p.get("ts")]
            dt_ms = int(round((ts[-1]-ts[0]) / max(1, len(ts)-1))) if len(ts) >= 2 else 100

            win = {
                "window_id": int(time.time()*1000) & 0xffffffff,
                "dt_ms": dt_ms,
                "points": [
                    {
                        "ts": p["ts"],
                        "ax": p["ax"], "ay": p["ay"], "az": p["az"],   # g
                        "yaw": p["yaw"], "pitch": p["pitch"], "roll": p["roll"]  # deg
                    }
                    for p in buf[wid]
                ],
                "meta": {"wand_id": wid}
            }
            sock.sendall((json.dumps(win) + "\n").encode())
            print(f"Laptop -> Ultra96: wid={wid} points={len(buf[wid])} dt={dt_ms}ms")


def main():
    global sock
    # 1) Connect TCP once
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((ULTRA96_IP, ULTRA96_PORT))
    print(f"Connected to Ultra96 at {ULTRA96_IP}:{ULTRA96_PORT}")

    # 2) MQTT
    cli = mqtt.Client(CallbackAPIVersion.VERSION1, client_id="relay-node")
    cli.connect(BROKER, PORT, 60)
    cli.on_message = on_message
    cli.subscribe([
        (T_WAND_MPU, 0),
        (T_WAND_CAST, 1),
    ])

    # 3) Start listener thread for U96 -> relay
    threading.Thread(target=listen_ultra96, args=(cli,), daemon=True).start()
    print("Relay running…")
    cli.loop_forever()

if __name__ == "__main__":
    main()
