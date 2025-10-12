# from socket import socket
import json, time, collections, threading, socket, os
import paho.mqtt.client as mqtt
import ssl

BROKER = "localhost"   # your laptop IP
PORT   = 1883            # your mosquitto port

# Topics
# Subscribed topics
T_WAND_HELLO = "wand/hello"
T_WAND_MPU = "wand/mpu"
T_WAND_CAST = "wand/cast"
T_WAND_BATTERY = "wand/batt"

#Topics to publish to
T_WAND1_SPELL = "wand1/spell"
T_WAND2_SPELL = "wand2/spell"

# MQTT message handler
def on_message(client, _, msg):
    print("Hi")
    return


def main():
    cli = mqtt.Client("Ultra96-client")
    cli.connect(BROKER, PORT, 60)
    cli.on_message = on_message
    cli.subscribe([
        (T_WAND_HELLO, 1),
        (T_WAND_MPU, 0),
        (T_WAND_CAST, 1),
    ])
    print("Waiting for message...")
    cli.loop_forever()


if __name__ == "__main__":
    main()