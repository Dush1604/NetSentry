
cat > backend/schema.sql << 'EOF'
-- NetSentry database schema
-- Run automatically on first container start (see docker-compose.yml init mount),
-- or manually via: docker exec -i netsentry-db psql -U netsentry -d netsentry < schema.sql

CREATE EXTENSION IF NOT EXISTS timescaledb;

CREATE TABLE IF NOT EXISTS devices (
    id SERIAL PRIMARY KEY,
    device_uuid TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    location TEXT,
    last_seen TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS metrics (
    time TIMESTAMPTZ NOT NULL,
    device_id TEXT NOT NULL,
    rssi INTEGER,
    heap_free INTEGER
);

SELECT create_hypertable('metrics', 'time', if_not_exists => TRUE);

INSERT INTO devices (device_uuid, name, location)
VALUES ('node_a', 'Node A', 'living_room')
ON CONFLICT (device_uuid) DO NOTHING;

INSERT INTO devices (device_uuid, name, location)
VALUES ('node_b', 'Node B', 'basement')
ON CONFLICT (device_uuid) DO NOTHING;

CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'admin',
    created_at TIMESTAMPTZ DEFAULT now()
);

EOF
