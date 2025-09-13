import paho.mqtt.client as mqtt

# Callback when the client connects to the broker
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to broker")
        client.subscribe("wand/mpu")  # same topic ESP32 publishes to
    else:
        print(f"Failed to connect, rc={rc}")

# Callback when a message is received
def on_message(client, userdata, msg):
    print(f"Received message on topic {msg.topic}: {msg.payload.decode()}")

# Replace with your laptop IP if ESP32 connects using it
broker_address = "172.20.10.4"  # example: laptop IP
broker_port = 1883

# Create MQTT client
client = mqtt.Client("LaptopSubscriber")
client.on_connect = on_connect
client.on_message = on_message

# Connect to the broker
client.connect(broker_address, broker_port, 60)

# Start the network loop and wait for messages
client.loop_forever()