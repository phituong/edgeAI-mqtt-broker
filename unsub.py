import paho.mqtt.client as mqtt
import time

BROKER = "192.168.0.100"
PORT = 1883
TOPIC = "test/msg"
CLIENT_ID = "test_sub1"

# MQTT client setup
client = mqtt.Client(CLIENT_ID)

# Event handlers
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to broker")
    else:
        print("Failed to connect, return code:", rc)

def on_message(client, userdata, message):
    print(f"Received message: {message.payload.decode()} on topic {message.topic}")

def subscribe_unsubscribe():
    while True:
        print(f"Subscribing to topic: {TOPIC}")
        client.subscribe(TOPIC)
        time.sleep(1)  # Wait for messages

        print(f"Unsubscribing from topic: {TOPIC}")
        client.unsubscribe(TOPIC)
        time.sleep(1)  # Wait before next cycle

# Assign event handlers
client.on_connect = on_connect
client.on_message = on_message

# Connect and start loop
client.connect(BROKER, PORT, 60)
client.loop_start()

# Run the subscribe/unsubscribe loop
subscribe_unsubscribe()
