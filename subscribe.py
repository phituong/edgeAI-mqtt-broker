import paho.mqtt.client as mqtt
import logging
import argparse

# Parse command-line arguments
parser = argparse.ArgumentParser(description="MQTT Subscriber")
parser.add_argument("--client-id", type=str, default="test_sub", help="Client ID for the MQTT subscriber")
parser.add_argument("--topic", type=str, default="test/msg/#", help="Topic to subscribe to")
args = parser.parse_args()

# Enable Debugging Logs
mqtt_log = logging.getLogger("mqtt")
mqtt_log.setLevel(logging.DEBUG)
logging.basicConfig(level=logging.DEBUG)

# Broker Configuration
BROKER_IP = "192.168.0.100"
BROKER_PORT = 1883
KEEP_ALIVE = 30
QOS = 1  # Change to 2 for exactly-once delivery if needed

# Callback: Connection Successful
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Connected to broker!")
        print(f"📡 Subscribing to topic: {args.topic}")
        client.subscribe(args.topic, qos=QOS)
    else:
        print(f"❌ Failed to connect, return code {rc}")

# Callback: Subscription Acknowledgment (SUBACK)
def on_subscribe(client, userdata, mid, granted_qos):
    print(f"✅ SUBACK received for message ID {mid}, Granted QoS: {granted_qos}")

# Callback: Message Received
def on_message(client, userdata, msg):
    print(f"📩 Received message: {msg.payload.decode()} on topic {msg.topic}")

# Callback: PUBREC (Subscriber confirms PUBLISH received)
def on_publish(client, userdata, mid):
    print(f"🔄 PUBREC sent for message ID {mid}")

# Callback: PUBREL (Broker sends PUBREL)
def on_pubrel(client, userdata, mid):
    print(f"📨 PUBREL received from broker for message ID {mid}")

# Callback: PUBCOMP (Final step of QoS 2 flow)
def on_pubcomp(client, userdata, mid):
    print(f"✅ PUBCOMP sent for message ID {mid}, QoS 2 cycle completed")

# Create MQTT Client with client_id from arguments
client = mqtt.Client(client_id=args.client_id, clean_session=True)

# Assign Callbacks
client.on_connect = on_connect
client.on_subscribe = on_subscribe
client.on_message = on_message
client.on_publish = on_publish
client.on_pubrel = on_pubrel
client.on_pubcomp = on_pubcomp

# Connect to Broker
client.connect(BROKER_IP, BROKER_PORT, KEEP_ALIVE)

print(f"MQTT Subscriber is running... Listening on topic: {args.topic}")
client.loop_forever()
