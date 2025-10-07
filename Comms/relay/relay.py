# from socket import socket
import json, time, collections, threading, socket, os
from bleak import cli
import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion
import ssl

BROKER = "172.20.10.5"   # your laptop IP
PORT   = 1883            # your mosquitto port

# ULTRA96_IP = "172.26.191.147"  # update if needed
ULTRA96_IP = "127.0.0.1"  # update if needed
ULTRA96_PORT = 5000
sock = None

# Topics
# T_WAND_CMD  = "wand/cmd"
T_WAND_HELLO = "wand/hello"
T_WAND_MPU = "wand/+/mpu"
T_WAND_CAST = "wand/+/cast"
T_SPELL_ID_FMT = "wand/{wid}/spell"


# T_U96_DRAW_IN  = "u96/draw/in"
# T_U96_DRAW_OUT = "u96/draw/out"
# T_U96_CAST_IN  = "u96/cast/in"

T_U96_MPU = "wand/u96/mpu"   # Ultra96 will subscribe here
T_U96_CAST = "wand/u96/cast"
T_U96_SPELL = "wand/u96/spell"

# TLS certs for Mosquitto
CA_CERT   = "../../certs/ca.crt"
CERT_FILE = "../../certs/server.crt"
KEY_FILE  = "../../certs/server.key"

# Buffer ~2s at 10Hz (adjust as you like)
WINDOW_LEN = 60

# one buffer per wand
buf = {}  # mac -> deque

#To parse wand ID from topic
def topic_wid(topic: str) -> int:
    # expects "wand/<int>/mpu" or "wand/<int>/cast"
    try:
        seg = topic.split('/')[1]
        wid = int(''.join(ch for ch in seg if ch.isdigit()))
        return wid
    except Exception as e:
        print(f"[wid] bad topic='{topic}' err={e}")
        return 0

# ID assignment state (runtime only; resets each run)
uid_to_id = {}
next_id = 0

def ensure_id_for_uid(uid: str) -> int:
    global next_id
    uid = str(uid).upper()
    if uid in uid_to_id:
        return uid_to_id[uid]
    if next_id >= 2:
        print(f"[assign] already have 2 wands, giving {uid} id {next_id} anyway")
    uid_to_id[uid] = next_id
    wid = next_id
    next_id += 1
    print(f"[assign] {uid} -> {wid}")
    return wid

# def load_map():
#     global mac_to_id, id_to_mac
#     if os.path.exists(MAP_FILE):
#         with open(MAP_FILE, "r") as f:
#             mac_to_id = json.load(f)
#     id_to_mac = {int(v): k for k, v in mac_to_id.items()}
#     print("[map] loaded:", mac_to_id)

# def save_map():
#     with open(MAP_FILE, "w") as f:
#         json.dump(mac_to_id, f)
#     print("[map] saved:", mac_to_id)

# def next_free_id():
#     # 2-player game: prefer 0 then 1
#     for cand in (0,1):
#         if cand not in id_to_mac:
#             return cand
#     # fallback: still allow more, but you probably only need 2
#     cand = 0
#     while cand in id_to_mac: cand += 1
#     return cand

# def ensure_id_for(mac:str) -> int:
#     mac = mac.upper()
#     if mac in mac_to_id:
#         return int(mac_to_id[mac])
#     wid = next_free_id()
#     mac_to_id[mac] = wid
#     id_to_mac[wid] = mac
#     save_map()
#     print(f"[assign] {mac} -> {wid}")
#     return wid

# def topic_mac(topic:str) -> str:
#     # "wand/AA:BB:CC:DD:EE:FF/mpu" -> AA:BB:...
#     return topic.split('/')[1].upper()

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

                # Expect: {"wand_id": 0/1, "spell_type":"C/W/T/S/Z/I", ...}
                wid    = int(msg.get("wand_id", 0))
                letter = str(msg.get("spell_type", "U"))[:1]

                cli.publish(f"wand/{wid}/spell", json.dumps({"spell": letter}), qos=1, retain=False)
                print(f"⬅️ U96 -> wand{wid}/spell: {letter}")
                
                cli.publish("u96/draw/out", json.dumps(msg), qos=0)
            except Exception as e:
                print("Error parsing Ultra96 message:", e)


# MQTT message handler
def on_message(cli, _, msg):
    global sock

    if msg.topic.endswith("/mpu"):
        wid = topic_wid(msg.topic)
        print(f"[mpu] topic={msg.topic} -> wid={wid}")
        pt  = json.loads(msg.payload.decode())
        # keep only the fields U96 needs
        p = {
            "ts":    int(pt.get("ts", 0)),
            "ax":    float(pt.get("accelx", 0.0)),
            "ay":    float(pt.get("accely", 0.0)),
            "az":    float(pt.get("accelz", 0.0)),
            "yaw":   float(pt.get("yaw", 0.0)),
            "pitch": float(pt.get("pitch", 0.0)),
            "roll":  float(pt.get("roll", 0.0)),
        }

        dq = buf.setdefault(wid, collections.deque(maxlen=WINDOW_LEN))
        dq.append(p)

        if len(dq) == WINDOW_LEN:
            ts = [q["ts"] for q in dq if q["ts"]]
            dt_ms = int(round((ts[-1] - ts[0]) / max(1, len(ts) - 1))) if len(ts) >= 2 else 50
            win = {
                "window_id": int(time.time()*1000) & 0xffffffff,
                "dt_ms": dt_ms,
                "points": list(dq),          # list, not deque
                "meta": {"wand_id": wid}
            }
            sock.sendall((json.dumps(win) + "\n").encode())
            cli.publish(T_U96_MPU, json.dumps(win), qos=0, retain=False)
            print(f"Relay -> U96: wid={wid} points={len(dq)} dt={dt_ms}ms")
            dq.clear()

    elif msg.topic.endswith("/cast"):
        wid = topic_wid(msg.topic)
        ct  = json.loads(msg.payload.decode())

        cast = {
            "type": "cast",
            "wand_id": wid,
            "strength": int(ct.get("strength", 1)),
            "ts": ct.get("ts", 0)
        }
        sock.sendall((json.dumps(cast) + "\n").encode())
        print(f"Relay -> U96 (cast): wid={wid} strength={cast['strength']}")
    
    elif msg.topic == T_WAND_HELLO:
        try:
            d = json.loads(msg.payload.decode())
            uid = str(d.get("uid","")).upper()
            if not uid:
                print("[hello] missing uid")
                return
            wid = ensure_id_for_uid(uid)
            # reply to *exactly* wand/<UID>/assign
            cli.publish(f"wand/{uid}/assign",
                        json.dumps({"wand_id": wid}),
                        qos=1, retain=False)
            print(f"[hello] replied wand/{uid}/assign -> {{'wand_id':{wid}}}")
        except Exception as e:
            print("[hello] error:", e)
        return


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
