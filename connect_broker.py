import paho.mqtt.client as mqtt
import logging

# Enable Debugging Logs
mqtt_log = logging.getLogger("mqtt")
mqtt_log.setLevel(logging.DEBUG)
logging.basicConfig(level=logging.DEBUG)

# Broker Configuration
BROKER_IP = "192.168.0.100"
BROKER_PORT = 1883
KEEP_ALIVE = 30
CLEAN_SESSION = True

# Publish Message Configuration
QOS = 1
TOPIC = "test/msg/"
MESSAGE = "test publish to subscriber"

# Callback: Connection Successful
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Connected to broker!")
        # Set retain flag to True so the broker retains this message
        client.publish(TOPIC, MESSAGE, qos=QOS, retain=True)
    else:
        print(f"❌ Failed to connect, return code {rc}")

# Callback: Message Published (Final PUBCOMP for QoS 2)
def on_publish(client, userdata, mid):
    print(f"✅ PUBCOMP (final) received for message ID {mid}")

# Create MQTT Client
client = mqtt.Client(client_id="test_pub", clean_session=CLEAN_SESSION)

# Assign Callbacks
client.on_connect = on_connect
client.on_publish = on_publish

# Connect to Broker
client.connect(BROKER_IP, BROKER_PORT, KEEP_ALIVE)

print("MQTT client is running...")
client.loop_forever()
