# IoT-Virtual-Drawing
IoT-based virtual drawing system that streams IMU motion data from an ESP32 + MPU6050 wand to a browser for real-time 2D trajectory visualization.

## Project Overview

The system captures motion data (IMU) on the ESP32, publishes it to the cloud, and pushes updates to a web client that renders a 2D path in real time.

### High-level Flow
1. **ESP32** reads IMU data (MPU6050) and publishes telemetry.
2. **AWS IoT / Rule** triggers a **Lambda** function.
3. **Lambda** forwards the data to connected browser clients (e.g., via WebSocket) and/or stores connection state.
4. **Web Client (`index.html`)** displays the live trajectory.

---

## Repository Contents

- `ESPcode2.ino` — ESP32 firmware (reads MPU6050 and publishes IMU telemetry)
- `lambda_function.py` — AWS Lambda function (process/forward incoming telemetry)
- `index.html` — Browser dashboard (renders the drawing/trajectory)
- `Virtual Drawing System Using Motion Tracking IoT.pdf` — Project presentation
- `Classification and monitoring of arm exercises using machine learning and wrist-worn band.pdf` — Reference / related material

---

## Requirements

### Hardware
- ESP32 development board
- MPU6050 IMU module
- Jumper wires

### Software
- Arduino IDE / PlatformIO (for ESP32)
- AWS account (IoT Core + Lambda + optional API Gateway WebSocket)
- Web browser (Chrome/Edge/Firefox)

---

## Setup 

### 1) Wire the MPU6050 to ESP32
Typical I2C wiring (verify your board pinout):
- VCC → 3.3V
- GND → GND
- SDA → ESP32 SDA
- SCL → ESP32 SCL

### 2) Configure and flash the ESP32 firmware
1. Open `ESPcode2.ino`
2. Set:
   - Wi-Fi SSID/password
   - AWS IoT endpoint / topics (as used by your cloud setup)
3. Install needed Arduino libraries (commonly: Wire, MPU6050 library, WiFi, MQTT client)
4. Upload to ESP32 and open Serial Monitor to confirm connection and publishing

### 3) Deploy the AWS Lambda
1. Create a Lambda function and paste `lambda_function.py`
2. Give the Lambda IAM permissions as needed (e.g., logs, DynamoDB, WebSocket management API if used)
3. Create an AWS IoT Rule to trigger the Lambda on the telemetry topic

> If you use WebSockets: create an API Gateway WebSocket API and configure your Lambda to post messages to connected clients.

### 4) Run the Web Dashboard
- Open `index.html` in a browser
- Configure the WebSocket endpoint / API URL inside the HTML/JS (if applicable)
- Confirm that incoming telemetry updates the drawing in real time

---

## Telemetry Format (Example)

Recommended JSON payload shape (adapt to your implementation):

```json
{
  "ts": 1700000000,
  "ax": 0.12,
  "ay": -0.03,
  "az": 9.78,
  "gx": 0.01,
  "gy": 0.02,
  "gz": -0.01
}
