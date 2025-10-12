# from socket import socket
import json, time, collections, threading, socket, os
import paho.mqtt.client as mqtt
from collections import deque

BROKER = "localhost"   # "localhost" if shift to ultra96, laptopIP if on laptop
PORT   = 1883            # your mosquitto port


# Global variables for testing 
WINDOW = 60
count = 0

# Global variables for game logic
# Locks to be acquired when modifying spell deque
connected = False
wand1IsReady = False
wand2IsReady = False
spells_lock = threading.Lock()
display_lock = threading.Lock()
player1_spells = deque(maxlen=5)
player2_spells = deque(maxlen=5)
UPDATE_INTERVAL = 0.5
battery_percent1 = None
battery_percent2 = None
wand1_drawingMode = True
wand2_drawingMode = False
wand1_spell = "U"
wand2_spell = "U"

# Topics
# Subscribed topics
T_WAND1_STATUS = "wand1/status" 
T_WAND2_STATUS = "wand2/status"
T_WAND1_BATT = "wand1/batt"
T_WAND2_BATT = "wand2/batt"
T_WAND1_MPU = "wand1/mpu"
T_WAND2_MPU = "wand2/mpu"
T_WAND1_CAST = "wand1/cast"
T_WAND2_CAST = "wand2/cast"

# Topic to publish to
T_U96_STATUS = "u96/status"
T_U96_WAND1_SPELL = "u96/wand1/spell"
T_U96_WAND2_SPELL = "u96/wand2/spell"

def on_connect(client, userdata, flags, rc):
    global connected
    connected = True
    client.subscribe([
        (T_WAND1_STATUS, 1),
        (T_WAND2_STATUS, 1),
        (T_WAND1_BATT, 1),
        (T_WAND2_BATT, 1),
        (T_WAND1_MPU, 0),
        (T_WAND2_MPU, 0),
        (T_WAND1_CAST, 2),
        (T_WAND2_CAST, 2),
    ])
    if battery_percent1 and battery_percent2:
        message = {
            "ready":True,
            "wand1_state": {
                "drawingMode":wand1_drawingMode,
                "spell":wand1_spell,
            },
            "wand2_state": {
                "drawingMode":wand2_drawingMode,
                "spell":wand2_spell,
            }
        }
        client.publish(T_U96_STATUS, json.dumps(message), 1, True)

def on_disconnect(client, userdata, rc):
    global connected
    connected = False

# MQTT message handler
def on_message(client, _, msg):
    topic = msg.topic
    msgJS = json.loads(msg.payload.decode())
    print(msg.payload.decode(), topic)
    if topic == T_WAND1_STATUS:
        global wand1IsReady
        wand1IsReady = msgJS["ready"]
        if wand1IsReady:
            time.sleep(5)
            message = {"ready":True}
            client.publish(T_WAND2_STATUS, json.dumps(message), 1, True)
            
    elif topic == T_WAND1_MPU:
        global count
        count += 1
        if count == WINDOW:
            message = {        # keep if you still use numeric somewhere
                "spell_type":"C",    # <-- single-letter for ESP
            }
            count = 0
            client.publish(T_U96_WAND1_SPELL, json.dumps(message), 2, False)
        

    elif topic == T_WAND1_BATT:
        global battery_percent1 
        battery_percent1 = msgJS["percent"]

    

def main():
    cli = mqtt.Client("Ultra96-client")
    # --- set Last Will before connecting ---
    will_message = json.dumps({
        "ready":False,
        "wand1_state": {
            "drawingMode":wand1_drawingMode,
            "spell":wand1_spell,
        },
        "wand2_state": {
            "drawingMode":wand2_drawingMode,
            "spell":wand2_spell,
        }
    })
    # topic, payload, qos, retain
    cli.will_set(T_U96_STATUS, payload=will_message, qos=1, retain=True)
    cli.on_connect = on_connect
    cli.on_message = on_message
    cli.on_disconnect = on_disconnect
    cli.connect(BROKER, PORT, 60)
    print("Waiting for message...")
    cli.loop_start()
    while(not battery_percent1):
        continue
    message = {
        "ready":True,
        "wand1_state": {
            "drawingMode":wand1_drawingMode,
            "spell":wand1_spell,
        },
        "wand2_state": {
            "drawingMode":wand2_drawingMode,
            "spell":wand1_spell,
        }
    }
    cli.publish(T_U96_STATUS, json.dumps(message), 1, True)
    
    while True:
        continue


if __name__ == "__main__":
    main()