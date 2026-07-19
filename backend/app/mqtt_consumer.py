
import os
import json
from datetime import datetime, timezone

import psycopg2
import paho.mqtt.client as mqtt
from dotenv import load_dotenv

load_dotenv()

DB_CONFIG = {
    "host": os.getenv("DB_HOST"),
    "port": os.getenv("DB_PORT"),
    "dbname": os.getenv("DB_NAME"),
    "user": os.getenv("DB_USER"),
    "password": os.getenv("DB_PASSWORD"),
}

MQTT_HOST = os.getenv("MQTT_BROKER_HOST")
MQTT_PORT = int(os.getenv("MQTT_BROKER_PORT"))
MQTT_TOPIC = "sentinel/#"


def get_db_connection():
    return psycopg2.connect(**DB_CONFIG)


def on_connect(client, userdata, flags, reason_code, properties):
    print(f"[MQTT] Connected with result code: {reason_code}")
    client.subscribe(MQTT_TOPIC)
    print(f"[MQTT] Subscribed to {MQTT_TOPIC}")


def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
        print(f"[MQTT] Received on {msg.topic}: {payload}")

        conn = get_db_connection()
        cur = conn.cursor()
        cur.execute(
            """
            INSERT INTO metrics (time, device_id, rssi, heap_free)
            VALUES (%s, %s, %s, %s)
            """,
            (
                datetime.now(timezone.utc),
                payload.get("node_id"),
                payload.get("rssi"),
                payload.get("heap_free"),
            ),
        )
        conn.commit()
        cur.close()
        conn.close()
        print(f"[DB] Inserted reading for {payload.get('node_id')}")

    except Exception as e:
        print(f"[ERROR] Failed to process message: {e}")


def main():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message

    print(f"[MQTT] Connecting to {MQTT_HOST}:{MQTT_PORT}...")
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_forever()


if __name__ == "__main__":
    main()
