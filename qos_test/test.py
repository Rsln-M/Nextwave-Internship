import paho.mqtt.client as mqtt
import time
import json
import base64

BROKER = "localhost"  # Change to your broker's IP if needed
TOPIC = "test/topic"
PORT = 1883
FILENAME1 = "firmware/firmware.jpeg"
FILENAME2 = "firmware/672ye1wud3r51.png"
OUTPUT_FILE = "firmware_received.png"

# Global variable to store start time
finish_time = 0
count = 0

def on_connect(client, userdata, flags, rc, properties):
    print(f"Connected with result code {rc}")
    client.subscribe(TOPIC)

def on_subscribe(client, userdata, mid, rc_list, properties):
    print(f"Subscribed to {TOPIC}")

def on_message(client, userdata, msg):
    global finish_time
    global count
    end_time = time.time()
    message = json.loads(msg.payload.decode())
    firmware_data = base64.b64decode(message['data'])
    print(f"Message with mid {msg.mid} was sent at: {message['timestamp']}")
    print(f"Message with mid {msg.mid} arrived at {end_time}")
    count += 1
    finish_time = end_time
    # try:
    #     # Save the received firmware data to a file
    #     with open(OUTPUT_FILE, 'wb') as file:
    #         file.write(firmware_data)
    #     print(f"Firmware saved to {OUTPUT_FILE}")
    # except Exception as e:
    #     print(f"Error saving firmware: {e}")
    # client.disconnect()

def on_publish(client, userdata, mid, rc, properties):
    print(f"Message published with mid {mid}")

# Subscriber
subscriber = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
subscriber.on_connect = on_connect
subscriber.on_message = on_message
subscriber.on_subscribe = on_subscribe
print(subscriber.connect(BROKER, PORT, 60))

# Publisher
publisher = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
publisher.on_publish = on_publish
publisher.connect(BROKER, PORT, 60)
# time.sleep(10)
# Start subscriber in a separate thread
subscriber.loop_start()

# Simulate delay before publishing
time.sleep(1)

# Record start time and publish message
with open(FILENAME2, "rb") as f:
    firmware_data = f.read()
publisher.loop_start()
start_time = time.time()
for i in range(100):
    publisher.publish(TOPIC, json.dumps({"data": base64.b64encode(firmware_data).decode(), "timestamp": time.time()}), qos = 0).wait_for_publish(10)
time.sleep(10)
print(f"total: {finish_time - start_time:.6f} seconds")
print(f"count: {count}")
# print(f"ave: {total_time/count}")
# Stop subscriber loop after disconnect
subscriber.loop_stop()