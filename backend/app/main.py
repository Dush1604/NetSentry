
import os
import json
import threading
import uuid
from datetime import datetime, timezone
from contextlib import asynccontextmanager

from fastapi import Depends, HTTPException, status, File, UploadFile
from fastapi.security import OAuth2PasswordRequestForm
from app.auth import hash_password, verify_password, create_access_token, decode_access_token

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
    client.subscribe("sentinel/+/telemetry")
    client.subscribe("sentinel/+/devices")
    # client.subscribe(MQTT_TOPIC)
    # print(f"[MQTT] Subscribed to {MQTT_TOPIC}")
    print(f"[MQTT] Subscribed to telemetry and devices topics")


def handle_telemetry(node_id, payload):
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


def handle_device_list(client, node_id, devices):
    conn = get_db_connection()
    cur = conn.cursor()

    for device in devices:
        mac = device.get("mac")
        ip = device.get("ip")

        cur.execute("SELECT label FROM known_devices WHERE mac_address = %s", (mac,))
        if cur.fetchone() is not None:
            continue  # trusted device, nothing to do

        # Check if we've already alerted on this MAC recently (last 24h)
        cur.execute(
            """
            SELECT id FROM events
            WHERE event_type = 'unrecognized_device'
              AND details->>'mac' = %s
              AND time > now() - INTERVAL '24 hours'
            """,
            (mac,),
        )
        if cur.fetchone() is not None:
            continue  # already alerted recently, skip

        details = json.dumps({"mac": mac, "ip": ip, "seen_from": node_id})
        cur.execute(
            "INSERT INTO events (device_id, event_type, details) VALUES (%s, %s, %s)",
            (node_id, "unrecognized_device", details),
        )
        event_id = cur.fetchone()[0]
        conn.commit()
        print(f"[ALERT] Unrecognized device {mac} ({ip}) seen from {node_id}")

        command_topic = f"sentinel/{node_id}/command"
        command_payload = json.dumps({"action": "capture_snapshot", "event_id": event_id})
        client.publish(command_topic, command_payload)
        print(f"[MQTT] Sent capture_snapshot command to {command_topic}")

    cur.close()
    conn.close()


def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
        topic_parts = msg.topic.split("/")  # e.g. ["sentinel", "node_a", "telemetry"]
        node_id = topic_parts[1]
        message_type = topic_parts[2]

        if message_type == "telemetry":
            handle_telemetry(node_id, payload)
        elif message_type == "devices":
            handle_device_list(client, node_id, payload)

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


IMAGE_STORAGE_DIR = "storage/images"

@app.post("/images/upload")
async def upload_image(
    device_id: str,
    event_id: int | None = None,
    file: UploadFile = File(...),
):
    filename = f"{device_id}_{uuid.uuid4().hex}.jpg"
    file_path = os.path.join(IMAGE_STORAGE_DIR, filename)

    with open(file_path, "wb") as f:
        content = await file.read()
        f.write(content)

    conn = get_db_connection()
    cur = conn.cursor()
    cur.execute(
        """
        INSERT INTO images (event_id, device_id, file_path)
        VALUES (%s, %s, %s)
        RETURNING id
        """,
        (event_id, device_id, file_path),
    )
    image_id = cur.fetchone()[0]
    conn.commit()
    cur.close()
    conn.close()

    print(f"[Image] Stored {filename} ({len(content)} bytes) for {device_id}")
    return {"image_id": image_id, "file_path": file_path}


@app.post("/auth/register")
def register(email: str, password: str):
    conn = get_db_connection()
    cur = conn.cursor()
    cur.execute("SELECT id FROM users WHERE email = %s", (email,))
    if cur.fetchone():
        cur.close()
        conn.close()
        raise HTTPException(status_code=400, detail="Email already registered")

    hashed = hash_password(password)
    cur.execute(
        "INSERT INTO users (email, password_hash) VALUES (%s, %s)",
        (email, hashed),
    )
    conn.commit()
    cur.close()
    conn.close()
    return {"message": "User created"}


@app.post("/auth/login")
def login(form_data: OAuth2PasswordRequestForm = Depends()):
    conn = get_db_connection()
    cur = conn.cursor()
    cur.execute("SELECT password_hash FROM users WHERE email = %s", (form_data.username,))
    row = cur.fetchone()
    cur.close()
    conn.close()

    if not row or not verify_password(form_data.password, row[0]):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Incorrect email or password",
        )

    token = create_access_token(data={"sub": form_data.username})
    return {"access_token": token, "token_type": "bearer"}

@app.get("/")
def root():
    return {"status": "NetSentry API running"}


@app.get("/metrics/latest")
def latest_metrics(limit: int = 10, current_user: dict = Depends(decode_access_token)):
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


@app.get("/events/recent")
def recent_events(limit: int = 20, current_user: dict = Depends(decode_access_token)):
    conn = get_db_connection()
    cur = conn.cursor()
    cur.execute(
        "SELECT id, time, device_id, event_type, details FROM events ORDER BY time DESC LIMIT %s",
        (limit,),
    )
    rows = cur.fetchall()
    cur.close()
    conn.close()
    return [
        {"id": r[0], "time": r[1].isoformat(), "device_id": r[2], "event_type": r[3], "details": r[4]}
        for r in rows
    ]
