import paho.mqtt.client as mqtt
import json
import csv
import os
import time
from collections import deque

# ------------------ CONFIG ------------------
'''
# Shree
broker_address = "172.20.10.5"  # example: laptop IP
'''

'''
#CK
broker_address = "172.20.10.5"  # example: laptop IP
'''
# KW
broker_address = "192.168.1.12"  # replace with your laptop IP
broker_port = 1883
topic1 = "wand/mpu"
topic2 = "wand/cast"
topic3 = "wand1/spell"
topic4 = "wand2/spell"
count = 0

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Connected to MQTT broker")
        client.subscribe(topic1)
        client.subscribe(topic2)
    else:
        print(f"❌ Failed to connect, rc={rc}")

def on_message(client, userdata, msg):
    try:
        if msg.topic == topic1:
            global count
            if count == 60:
                client.publish(topic3, json.dumps({"spell": "I"}))
                count = 0
            else:
                data = json.loads(msg.payload.decode())
                row = [
                    float(data["yaw"]),
                    float(data["pitch"]),
                    float(data["roll"]),
                    float(data["accelx"]),
                    float(data["accely"]),
                    float(data["accelz"])
                ]
                
                # Print each reading
                print(f"Yaw: {row[0]:.2f}, Pitch: {row[1]:.2f}, Roll: {row[2]:.2f}, "
                        f"AccelX: {row[3]:.2f}, AccelY: {row[4]:.2f}, AccelZ: {row[5]:.2f}")
                count += 1

        else:
            data = json.loads(msg.payload.decode())
            print(data["strength"])
    

    except json.JSONDecodeError:
        print("Invalid message:", msg.payload)
    except KeyError as e:
        print("Missing key in message:", e)


# Create MQTT client
client = mqtt.Client("LaptopSubscriber")
client.on_connect = on_connect
client.on_message = on_message
client.connect(broker_address, broker_port, 60)

client.loop_start()

try:
    while True:
        time.sleep(0.1)  # main thread idle
except KeyboardInterrupt:
    print("Exiting...")
    client.loop_stop()
