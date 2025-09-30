import paho.mqtt.client as mqtt
import json
import csv
import os
import time
from collections import deque

# ------------------ CONFIG ------------------

# Shree
broker_address = "172.20.10.5"  # example: laptop IP


'''
#CK
broker_address = "172.20.10.5"  # example: laptop IP
'''
'''
# KW
broker_address = "172.20.10.4"  # replace with your laptop IP
'''

broker_port = 1883
topic = "wand/mpu"
sampling_rate = 20 # Hz, MPU6050 DMP output
window_seconds = 3
window_size = sampling_rate * window_seconds  # 3 seconds -> 300 readings
save_folder = "wand_dataset/data/shree"  # folder to save CSV samples
wand_classes = ["Wave", "Circle", "Square", "Triangle", "Infinity", "Zigzag", "None"]
current_label = "Circle"  # Set this before recording a gesture
# -------------------------------------------

# Ensure save folder exists
os.makedirs(save_folder, exist_ok=True)

# Buffer to hold a 3-second window
buffer = deque(maxlen=window_size)

# Counter for saved samples
sample_counter = 0

def save_window_to_csv(window, label):
    """Save a 3-second window to CSV"""
    global sample_counter
    filename = f"{label}_{int(time.time())}_{sample_counter}.csv"
    filepath = os.path.join(save_folder, filename)
    
    with open(filepath, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["yaw","pitch","roll","accelx","accely","accelz"])  # header
        writer.writerows(window)
    
    sample_counter += 1
    print(f"\n✅ Saved sample {sample_counter}: {filepath}\n")

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Connected to MQTT broker")
        client.subscribe(topic)
    else:
        print(f"❌ Failed to connect, rc={rc}")

def on_message(client, userdata, msg):
    global buffer, current_label
    try:
        data = json.loads(msg.payload.decode())
        row = [
            float(data["yaw"]),
            float(data["pitch"]),
            float(data["roll"]),
            float(data["accelx"]),
            float(data["accely"]),
            float(data["accelz"])
        ]
        buffer.append(row)
        
        # Print each reading
        print(f"Yaw: {row[0]:.2f}, Pitch: {row[1]:.2f}, Roll: {row[2]:.2f}, "
              f"AccelX: {row[3]:.2f}, AccelY: {row[4]:.2f}, AccelZ: {row[5]:.2f}")
        
        # Save window automatically when full
        if len(buffer) == window_size and current_label in wand_classes:
            save_window_to_csv(list(buffer), current_label)
            buffer.clear()  # reset for next sample

    except json.JSONDecodeError:
        print("Invalid message:", msg.payload)
    except KeyError as e:
        print("Missing key in message:", e)


# Create MQTT client
client = mqtt.Client("LaptopSubscriber")
client.on_connect = on_connect
client.on_message = on_message
client.connect(broker_address, broker_port, 60)

print("➡️  Set current_label to the gesture you want to record before moving the wand.")
print(f"Available labels: {wand_classes}")
print("Press Ctrl+C to stop the program.")

client.loop_start()

try:
    while True:
        time.sleep(0.1)  # main thread idle
except KeyboardInterrupt:
    print("Exiting...")
    client.loop_stop()
