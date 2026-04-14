# Hospital Board MQTT Integration Guide

Complete guide to consuming MQTT data from the Smart Hospital Monitor Board and integrating it into your workflows.

---

## Table of Contents
1. [MQTT Broker Connection](#mqtt-broker-connection)
2. [Data Format & Topics](#data-format--topics)
3. [Publishing Behavior](#publishing-behavior)
4. [Integration Examples](#integration-examples)
5. [Monitoring & Debugging](#monitoring--debugging)
6. [Workflow Examples](#workflow-examples)

---

## MQTT Broker Connection

### Broker Details
- **Host**: `e16a2ae3d3034163bb42dceb064fe375.s1.eu.hivemq.cloud`
- **Port**: `8883` (TLS/SSL required)
- **Username**: `FUSION_AI`
- **Password**: `Aa12345678`
- **Protocol**: MQTT over TLS (MQTTs)

### Connection Requirements
- TLS/SSL Support required (port 8883)
- Username and password authentication
- Client ID (any unique identifier)

---

## Data Format & Topics

### Topic Structure
```
hospital/sensor/data
```

### Published JSON Format

#### Normal/Alert State (Every 60 seconds or on state change)
```json
{
  "device_id": "SmartHospitalMonitor_01",
  "temperature": 25.5,
  "humidity": 65.3,
  "pressure": 1013.2,
  "alert_status": "NORMAL",
  "emergency_status": "OK",
  "timestamp": 123456789
}
```

#### Emergency State (Published Immediately)
```json
{
  "device_id": "SmartHospitalMonitor_01",
  "temperature": 25.5,
  "humidity": 65.3,
  "pressure": 1013.2,
  "alert_status": "ALERT",
  "emergency_status": "EMERGENCY",
  "timestamp": 123456789
}
```

### Field Descriptions

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `device_id` | string | - | Unique device identifier |
| `temperature` | float | 0-100°C | Current temperature reading |
| `humidity` | float | 0-100% | Current humidity percentage |
| `pressure` | float | 800-1200 hPa | Atmospheric pressure |
| `alert_status` | string | "NORMAL" / "ALERT" | Temperature alert status |
| `emergency_status` | string | "OK" / "EMERGENCY" | Emergency button status |
| `timestamp` | int | milliseconds | Device uptime (ms since boot) |

---

## Publishing Behavior

### Normal Operation
- **Frequency**: Every **60 seconds**
- **Status**: `"alert_status": "NORMAL"`, `"emergency_status": "OK"`
- **Use Case**: Regular monitoring, data logging, dashboards

### Alert State (Temperature Threshold Exceeded)
- **Trigger**: Temperature > 28.0°C
- **Frequency**: **Immediate** (then every 60 seconds)
- **Status**: `"alert_status": "ALERT"`, `"emergency_status": "OK"`
- **LED Indicator**: Red LED activates
- **Audio**: Buzzer pulses every 2 seconds
- **Use Case**: Send notifications, trigger alarms

### Emergency State (SOS Button Pressed)
- **Trigger**: Physical emergency button press
- **Frequency**: **Immediate** (then every 60 seconds)
- **Status**: `"alert_status": varies`, `"emergency_status": "EMERGENCY"`
- **LED Indicator**: Red LED activates
- **Audio**: Buzzer sounds immediately
- **Use Case**: Critical alert, emergency dispatch, urgent notification

---

## Integration Examples

### Python - MQTT Subscriber

#### Basic Subscriber
```python
import paho.mqtt.client as mqtt
import json
import ssl

# MQTT Configuration
BROKER = "e16a2ae3d3034163bb42dceb064fe375.s1.eu.hivemq.cloud"
PORT = 8883
USERNAME = "FUSION_AI"
PASSWORD = "Aa12345678"
TOPIC = "hospital/sensor/data"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✓ Connected to MQTT Broker")
        client.subscribe(TOPIC)
    else:
        print(f"✗ Connection failed with code {rc}")

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        print(f"\n📊 Device: {data['device_id']}")
        print(f"🌡️  Temperature: {data['temperature']}°C")
        print(f"💧 Humidity: {data['humidity']}%")
        print(f"🔽 Pressure: {data['pressure']} hPa")
        print(f"🚨 Alert: {data['alert_status']}")
        print(f"⚠️  Emergency: {data['emergency_status']}")
        
        # Handle alerts
        if data['alert_status'] == 'ALERT':
            print("⚠️ TEMPERATURE ALERT!")
        if data['emergency_status'] == 'EMERGENCY':
            print("🚨 EMERGENCY EVENT!")
    except Exception as e:
        print(f"Error parsing message: {e}")

# Initialize MQTT Client
client = mqtt.Client(client_id="python-subscriber")
client.on_connect = on_connect
client.on_message = on_message

# Configure TLS
client.tls_set(ca_certs=None, certfile=None, keyfile=None, cert_reqs=ssl.CERT_NONE)
client.tls_insecure_set(True)

# Set credentials
client.username_pw_set(USERNAME, PASSWORD)

# Connect
client.connect(BROKER, PORT, keepalive=60)
client.loop_forever()
```

#### Advanced with Data Logging
```python
import paho.mqtt.client as mqtt
import json
import ssl
from datetime import datetime
import csv

class HospitalBoardMonitor:
    def __init__(self):
        self.data_log = []
        self.alerts = []
        
    def on_message(self, client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode())
            self.process_data(data)
        except Exception as e:
            print(f"Error: {e}")
    
    def process_data(self, data):
        # Log all data
        self.data_log.append({
            'timestamp': datetime.now().isoformat(),
            **data
        })
        
        # Alert handling
        if data['alert_status'] == 'ALERT':
            self.handle_alert(data)
        
        if data['emergency_status'] == 'EMERGENCY':
            self.handle_emergency(data)
    
    def handle_alert(self, data):
        alert = {
            'type': 'TEMPERATURE',
            'temperature': data['temperature'],
            'timestamp': datetime.now().isoformat()
        }
        self.alerts.append(alert)
        print(f"🔔 Alert logged: Temp {data['temperature']}°C")
    
    def handle_emergency(self, data):
        alert = {
            'type': 'EMERGENCY',
            'timestamp': datetime.now().isoformat()
        }
        self.alerts.append(alert)
        print(f"🚨 EMERGENCY ALERT TRIGGERED")
    
    def save_logs(self, filename='hospital_board_log.csv'):
        if self.data_log:
            with open(filename, 'w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=self.data_log[0].keys())
                writer.writeheader()
                writer.writerows(self.data_log)

# Usage
monitor = HospitalBoardMonitor()
# ... connect and run ...
```

---

### Node.js - MQTT Subscriber

```javascript
const mqtt = require('mqtt');

// MQTT Connection Options
const options = {
  host: 'e16a2ae3d3034163bb42dceb064fe375.s1.eu.hivemq.cloud',
  port: 8883,
  protocol: 'mqtts',
  username: 'FUSION_AI',
  password: 'Aa12345678',
  clientId: 'nodejs-subscriber',
  rejectUnauthorized: false
};

// Connect to broker
const client = mqtt.connect(options);

client.on('connect', () => {
  console.log('✓ Connected to MQTT Broker');
  client.subscribe('hospital/sensor/data', (err) => {
    if (!err) {
      console.log('✓ Subscribed to hospital/sensor/data');
    }
  });
});

client.on('message', (topic, message) => {
  try {
    const data = JSON.parse(message.toString());
    
    console.log('\n📊 Hospital Monitor Data:');
    console.log(`Device: ${data.device_id}`);
    console.log(`Temp: ${data.temperature}°C`);
    console.log(`Humidity: ${data.humidity}%`);
    console.log(`Pressure: ${data.pressure} hPa`);
    console.log(`Alert: ${data.alert_status}`);
    console.log(`Emergency: ${data.emergency_status}`);
    
    // Alert handling
    if (data.alert_status === 'ALERT') {
      handleAlert(data);
    }
    
    if (data.emergency_status === 'EMERGENCY') {
      handleEmergency(data);
    }
  } catch (error) {
    console.error('Error parsing message:', error);
  }
});

function handleAlert(data) {
  console.log('⚠️ TEMPERATURE ALERT! Sending notification...');
  // Send email, SMS, webhook, etc.
}

function handleEmergency(data) {
  console.log('🚨 EMERGENCY DETECTED! Calling emergency services...');
  // Trigger emergency protocol
}

client.on('error', (err) => {
  console.error('Connection error:', err);
});
```

---

### MQTT Explorer (GUI Tool)

1. **Download**: [MQTT Explorer](http://mqtt-explorer.com/)
2. **Configure Connection**:
   - Host: `e16a2ae3d3034163bb42dceb064fe375.s1.eu.hivemq.cloud`
   - Port: `8883`
   - Protocol: `mqtts`
   - Username: `FUSION_AI`
   - Password: `Aa12345678`
3. **Subscribe**: Click `+` → Topic filter: `hospital/sensor/data`
4. **Monitor**: Watch real-time messages arrive

---

## Monitoring & Debugging

### Using `mosquitto_sub` (Command Line)

```bash
# Subscribe and display messages
mosquitto_sub \
  -h e16a2ae3d3034163bb42dceb064fe375.s1.eu.hivemq.cloud \
  -p 8883 \
  -u FUSION_AI \
  -P Aa12345678 \
  --cafile /path/to/ca.crt \
  -t "hospital/sensor/data" \
  -v
```

### Expected Output
```
hospital/sensor/data {"device_id":"SmartHospitalMonitor_01","temperature":25.5,"humidity":65.3,"pressure":1013.2,"alert_status":"NORMAL","emergency_status":"OK","timestamp":123456789}
hospital/sensor/data {"device_id":"SmartHospitalMonitor_01","temperature":28.5,"humidity":68.1,"pressure":1013.4,"alert_status":"ALERT","emergency_status":"OK","timestamp":123460000}
```

---

## Workflow Examples

### Workflow 1: Temperature Monitoring Dashboard

```
Graph Display (Web Dashboard)
    ↑
    | (Every 60 sec)
    |
MQTT Broker ←── Hospital Board
    ↓
Database (InfluxDB / TimescaleDB)
    ↓
Grafana Dashboard
```

**Python Implementation**:
```python
from influxdb import InfluxDBClient

client = InfluxDBClient(host='localhost', port=8086, database='hospital')

def on_message(client, userdata, msg):
    data = json.loads(msg.payload.decode())
    
    point = {
        "measurement": "hospital_monitor",
        "tags": {"device_id": data['device_id']},
        "fields": {
            "temperature": data['temperature'],
            "humidity": data['humidity'],
            "pressure": data['pressure']
        },
        "time": data['timestamp']
    }
    
    client.write_points([point])
```

---

### Workflow 2: Alert Notification System

```
Emergency Detected
    ↓
MQTT Message (emergency_status: EMERGENCY)
    ↓
Alert Handler
    ├→ Send Email
    ├→ Send SMS
    ├→ Call Webhook
    └→ Log to Database
```

**Python Implementation**:
```python
import smtplib
from email.mime.text import MIMEText
import requests

def send_email_alert(data):
    msg = MIMEText(f"EMERGENCY ALERT!\nTemp: {data['temperature']}°C")
    msg['Subject'] = "🚨 Hospital Board Emergency Alert"
    msg['From'] = "alert@hospital.com"
    msg['To'] = "admin@hospital.com"
    
    with smtplib.SMTP('smtp.gmail.com', 587) as server:
        server.starttls()
        server.login("user@gmail.com", "password")
        server.send_message(msg)

def send_webhook_alert(data):
    requests.post(
        'https://your-api.com/alert',
        json={
            'type': 'emergency',
            'data': data
        }
    )

def on_emergency(data):
    send_email_alert(data)
    send_webhook_alert(data)
    log_to_database(data)
```

---

### Workflow 3: Real-time Alert + Data Logging

```
MQTT Broker
    ├→ Alert Handler (Emergency/Temperature Alert)
    │   ├→ Send Notification
    │   └→ Trigger Actions
    │
    └→ Data Logger (All Messages)
        └→ Store in Time-Series DB
            └→ Historical Analysis
```

---

### Workflow 4: Integration with Slack/Discord

**Python with Slack**:
```python
from slack_sdk import WebClient
from slack_sdk.errors import SlackApiError

slack_client = WebClient(token="xoxb-your-slack-token")

def send_slack_alert(data):
    try:
        if data['emergency_status'] == 'EMERGENCY':
            response = slack_client.chat_postMessage(
                channel="#hospital-alerts",
                text=f"🚨 EMERGENCY ALERT!",
                blocks=[
                    {
                        "type": "section",
                        "text": {
                            "type": "mrkdwn",
                            "text": f"*Emergency Alert* from {data['device_id']}\n" +
                                   f"Temperature: {data['temperature']}°C\n" +
                                   f"Humidity: {data['humidity']}%\n" +
                                   f"Status: {data['emergency_status']}"
                        }
                    }
                ]
            )
    except SlackApiError as e:
        print(f"Error: {e}")

def on_message(client, userdata, msg):
    data = json.loads(msg.payload.decode())
    send_slack_alert(data)
```

---

## Troubleshooting

### Connection Issues

| Problem | Solution |
|---------|----------|
| Connection refused | Check broker is online, verify IP/port |
| Authentication failed | Verify username/password are correct |
| TLS error | Ensure port 8883 (not 1883), set `tls_insecure_set(True)` for testing |
| Topic not receiving | Check subscription topic matches exact name |

### Debug Checklist

```bash
# 1. Test connectivity
telnet e16a2ae3d3034163bb42dceb064fe375.s1.eu.hivemq.cloud 8883

# 2. Check MQTT logs
mosquitto_sub -h <broker> -u <user> -P <pass> -t "#" --verbose

# 3. Monitor device output
# Check ESP32 serial monitor for MQTT publish messages
```

---

## Data Storage Recommendations

### Option 1: InfluxDB (Time-Series)
```
Best for: Temperature trends, historical analysis
Period: Keep 1 year data, 1 minute resolution
```

### Option 2: PostgreSQL + TimescaleDB
```
Best for: Long-term storage, complex queries
Period: Keep unlimited data, compress old data
```

### Option 3: MySQL + JSON Columns
```
Best for: Easy setup, JSON queries
Period: Keep 6 months active, archive older
```

---

## Performance Considerations

- **Message Size**: ~150 bytes per message
- **Frequency (Normal)**: 1 message per 60 seconds = ~8.6 KB/day
- **Frequency (Alert)**: Variable, assume 10 messages/minute = ~1.5 MB/day
- **Bandwidth**: Minimal, suitable for metered connections
- **Latency**: <100ms typical, <500ms worst case

---

## Security Best Practices

1. **Rotate Credentials**: Change FUSION_AI password regularly
2. **Use TLS Only**: Always port 8883, never 1883
3. **Firewall Rules**: Restrict broker access to known IPs if possible
4. **Rate Limiting**: Monitor for abnormal message frequency
5. **Data Encryption**: Encrypt sensitive data before storing
6. **Access Control**: Use ACLs to restrict topic access by device

---

## Support & Resources

- **HiveMQ Documentation**: https://www.hivemq.com/
- **MQTT Specification**: https://mqtt.org/
- **Paho MQTT (Python)**: https://github.com/eclipse/paho.mqtt.python
- **MQTT.js (Node.js)**: https://github.com/mqttjs/MQTT.js
- **Hospital Board GitHub**: [Your project repo]

---

**Last Updated**: April 14, 2026  
**Firmware Version**: Smart Hospital Monitor v1.0  
**MQTT Protocol Version**: 3.1.1
