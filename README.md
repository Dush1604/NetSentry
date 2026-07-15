
# NetSentry
Distributed IoT platform using dual ESP32-CAM nodes to correlate home network health (latency, signal strength, rogue devices) with physical motion events — FastAPI backend, JWT auth, TimescaleDB, Dockerized deployment.

** Status: ** early development - architecture and setup docs coming as each phase lands.

## Stack
- Firmware: ESP32
- Backend: FastAPI, MQTT (Mosquitto), PostgreSQL + TimescaleDB
- Dashboard: React
- Deployment: Docker Compose, GitHub Actions CI

## License
MIT - see [LICENSE](LICENSE)
