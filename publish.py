import paho.mqtt.client as mqtt
import argparse
import json

# Parse command-line arguments for key_number and status
parser = argparse.ArgumentParser(description="Publish and Subscribe to an MQTT topic.")
parser.add_argument("key_number", type=int, help="The key_number value")
parser.add_argument("status", type=int, help="The status value")
args = parser.parse_args()

# Create a dictionary to hold the JSON data
data = {
    "key_number": args.key_number,
    "status": args.status
}

# Convert the dictionary to a JSON string
message = json.dumps(data)

# Define the callback function for handling incoming messages
def on_message(client, userdata, msg):
    print(f"Received message: {msg.payload.decode()} on topic: {msg.topic}")

# Define the callback function for when the client connects to the broker
def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")

    # Once connected, subscribe to a different topic (change this topic as needed)
    topic_to_subscribe = '/congtac/5e8e365086ae62c9aaea6942a19fda7d50054/tinhtrangthietbi'
    print(f"Subscribing to topic: {topic_to_subscribe}")
    client.subscribe(topic_to_subscribe)

# Create an MQTT client
client = mqtt.Client()

# Set up callbacks
#client.on_connect = on_connect
#client.on_message = on_message

# Connect to the broker
client.connect("mqtt.eclipseprojects.io", 1883, 60)

# Start a new thread to handle the MQTT client
#client.loop_start()

# Publish the JSON message to the original topic
publish_topic = '/congtac/5e8e365086ae62c9aaea6942a19fda7d50054/dieukhien'
#publish_topic = '/congtac/5e8e365086ae62c9aaea6942a19fda7d50054/tinhtrangthietbi'
#publish_topic = '/test/topic'

client.publish(publish_topic, message, qos=0)

# Keep the script running to listen for incoming messages
input("Press Enter to exit...\n")

# Disconnect the client after everything is done
client.loop_stop()
client.disconnect()

print(f"Message '{message}' published to '{publish_topic}'")
