import json, time, collections, struct
import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion

BROKER = "172.20.10.5"   # your laptop IP
PORT   = 1884            # your mosquitto port

# Topics
T_WAND_IMU7 = "wand/imu7"
T_WAND_CMD  = "wand/cmd"
T_WAND_CAST = "wand/cast"

T_U96_DRAW_IN  = "u96/draw/in"
T_U96_DRAW_OUT = "u96/draw/out"
T_U96_CAST_IN  = "u96/cast/in"

# Buffer ~2s at 10Hz (adjust as you like)
WINDOW_LEN = 20
buf = collections.deque(maxlen=WINDOW_LEN)

# Takes the 7-byte message from ESP32 IMU and converts the raw ax/ay/az to g's
def imu7_to_point(payload7: bytes):
    """payload7 = [flags, axLE, ayLE, azLE] -> dict with floats in g"""
    if len(payload7) != 7: return None
    flags = payload7[0]
    ax, ay, az = struct.unpack("<hhh", payload7[1:7])
    drawing = (flags >> 1) & 1
    endbit  = flags & 1
    wand_id = (flags >> 2) & 1
    return {
        "ax": ax/16384.0, "ay": ay/16384.0, "az": az/16384.0,
        "drawing": bool(drawing), "end": bool(endbit), "wand_id": wand_id
    }

# MQTT message handler
def on_message(cli, _, msg):
    t = msg.topic

    if t == T_WAND_IMU7:
        pt = imu7_to_point(msg.payload)
        if not pt: return
        buf.append({"ax": pt["ax"], "ay": pt["ay"], "az": pt["az"]})
        if len(buf) == buf.maxlen:
            window_id = int(time.time()*1000) & 0xffffffff
            win = {
                "window_id": window_id,
                "dt_ms": 100,                   # 10Hz
                "points": list(buf),
                "meta": {"wand_id": pt["wand_id"]}
            }
            cli.publish(T_U96_DRAW_IN, json.dumps(win), qos=1)
            # --- Fake Ultra96 result so your flow works now ---
            fake = {"window_id": window_id, "spell_id": 2, "conf": 0.90, "stable": True}
            cli.publish(T_U96_DRAW_OUT, json.dumps(fake), qos=1)

    elif t == T_U96_DRAW_OUT:
        # Relay U96 result back to ESP32 as a /cmd
        r = json.loads(msg.payload.decode())
        cmd = {
            "cmd": "spell",
            "spell_id": int(r.get("spell_id", 0)),
            "conf": float(r.get("conf", 1.0)),
            "flags": {"armed": True, "stable": bool(r.get("stable", False))}
        }
        cli.publish(T_WAND_CMD, json.dumps(cmd), qos=1)

    elif t == T_WAND_CAST:
        # Forward cast info to U96 (or a game engine) for scoring
        cli.publish(T_U96_CAST_IN, msg.payload, qos=1)

def main():
    cli = mqtt.Client(CallbackAPIVersion.VERSION1, client_id="relay-node")
    cli.connect(BROKER, PORT, 60)
    cli.on_message = on_message
    cli.subscribe([
        (T_WAND_IMU7, 0),
        (T_U96_DRAW_OUT, 1),
        (T_WAND_CAST, 1),
    ])
    print("Relay running…")
    cli.loop_forever()

if __name__ == "__main__":
    main()
