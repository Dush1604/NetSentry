
import os
import json
import threading
from datetime import datetime, timezone
from contextlib import asynccontextmanager

import psycopg2
import paho.mqtt.client as mqtt
from dotenv import load_dotenv
from fastapi import FastAPI

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


def start_mqtt_consumer():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_forever()


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup: launch the MQTT consumer in a background thread
    mqtt_thread = threading.Thread(target=start_mqtt_consumer, daemon=True)
    mqtt_thread.start()
    print("[App] MQTT consumer thread started")
    yield
    # Shutdown: nothing to clean up yet — daemon thread dies with the process
    print("[App] Shutting down")


app = FastAPI(title="NetSentry API", lifespan=lifespan)


@app.get("/")
def root():
    return {"status": "NetSentry API running"}


@app.get("/metrics/latest")
def latest_metrics(limit: int = 10):
    conn = get_db_connection()
    cur = conn.cursor()
    cur.execute(
        "SELECT time, device_id, rssi, heap_free FROM metrics ORDER BY time DESC LIMIT %s",
        (limit,),
    )
    rows = cur.fetchall()
    cur.close()
    conn.close()
    return [
        {"time": r[0].isoformat(), "device_id": r[1], "rssi": r[2], "heap_free": r[3]}
        for r in rows
    ]
