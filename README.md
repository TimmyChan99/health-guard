# Smart Hospital Monitor - Application Guide

Complete reference for all functions, configuration options, and step-by-step usage instructions for the Smart Hospital Monitor firmware.

---

## Table of Contents
1. [System Architecture](#system-architecture)
2. [Setup & Initialization](#setup--initialization)
3. [Core Functions Reference](#core-functions-reference)
4. [Data Structures](#data-structures)
5. [Configuration & Customization](#configuration--customization)
6. [Step-by-Step Workflow](#step-by-step-workflow)
7. [Advanced Usage](#advanced-usage)

---

## System Architecture

### Hardware Configuration
```
ESP32 Development Board
├── Display (TM1637 - 4-digit 7-segment)
│   ├── GPIO 18: Clock
│   └── GPIO 5: Data
├── Sensors (I2C Bus)
│   ├── GPIO 21 (SDA): Data line
│   ├── GPIO 22 (SCL): Clock line
│   └── BME280: Temperature, Humidity, Pressure
├── Indicators
│   ├── GPIO 32: Green LED (normal)
│   ├── GPIO 33: Red LED (alert)
│   └── GPIO 26: Buzzer
└── Inputs
    ├── GPIO 35: Emergency SOS Button
    └── GPIO 34: Shutdown Button
```

### Software Stack
```
Main Application (main.cpp)
├── WiFi Management (WiFiManager)
├── MQTT Communication (PubSubClient)
├── Sensor Reading (Adafruit BME280)
├── Display Driver (NextTM1637)
└── Alert System (LEDs, Buzzer)
```

---

## Setup & Initialization

### Power-On Sequence

```
1. Serial Communication (115200 baud)
   ↓
2. WiFi Manager Initialization
   ├─ Check for saved credentials
   ├─ Auto-connect if available
   └─ Open setup portal if needed
   ↓
3. Pin Configuration (LEDs, Buzzer, Buttons)
   ↓
4. Display Initialization (TM1637)
   ├─ Set brightness level
   └─ Show startup message (0000)
   ↓
5. Sensor Initialization (BME280)
   ├─ Try address 0x76
   ├─ Try address 0x77
   └─ Continue if not found
   ↓
6. MQTT Configuration
   ├─ Set broker address/port
   └─ Attempt initial connection
   ↓
7. Ready (Display shows 8888)
```

### Initial Display Messages
- `0000` - System initializing
- `1111` - BME280 sensor not found (continues anyway)
- `8888` - System ready for operation

---

## Core Functions Reference

### 1. `setup()` - System Initialization

**Purpose**: Configure all hardware and establish connections on startup

**Called**: Once at power-on

**Flow**:
```cpp
void setup() {
    Serial.begin(115200);           // Initialize serial at 115200 baud
    delay(1000);                     // Wait for stability
    Serial.println("Starting...");   // Log startup
}
```

**Steps**:
1. Initialize serial communication
2. Create WiFiManager instance
3. Configure WiFi callbacks
4. Attempt WiFi auto-connect (180s timeout)
5. Initialize GPIO pins (mode, initial state)
6. Initialize TM1637 display
7. Initialize BME280 sensor (dual address check)
8. Configure MQTT broker connection
9. Display ready state (8888)

**Key Variables Modified**:
- `systemState.*` - All state variables initialized
- `currentSensorData.*` - Set to default values
- GPIO registers - Configured via pinMode()

**Troubleshooting**:
- Display shows `1111` → Sensor not found, check I2C wiring
- WiFi portal appears → No saved credentials, connect to `HospitalBoard-Setup`

---

### 2. `loop()` - Main Control Loop

**Purpose**: Continuously check all systems and manage operations

**Called**: Repeatedly, ~100 times per second (10ms delay)

**Execution Order**:
```
1. WiFi Status Check (every 10 seconds)
   ├─ If disconnected: log warning
   └─ If connected: log IP and signal strength
   
2. MQTT Connection Check (only if WiFi connected)
   ├─ If disconnected: attempt reconnection
   └─ If connected: process any received messages
   
3. Buzzer Timeout Check
   ├─ If buzzer active and duration expired
   └─ Turn off buzzer
   
4. Sensor Reading (every 3 seconds)
   ├─ Read temperature, humidity, pressure
   ├─ Update display
   └─ Check temperature limits
   
5. Button Monitoring (continuous)
   ├─ Check emergency button
   └─ Check shutdown button
   
6. Alert System Update
   ├─ Update LED colors
   └─ Trigger buzzer if needed
   
7. MQTT Publishing (conditional)
   ├─ Normal: every 60 seconds
   ├─ On state change: immediate
   └─ Emergency: immediate
   
8. Delay (10ms) to prevent watchdog trigger
```

**Critical Conditions**:
```cpp
// WiFi check condition
if (millis() - lastWiFiCheck > 10000) { ... }

// Sensor read condition  
if (millis() - systemState.lastSensorReadTime >= SENSOR_READ_INTERVAL) { ... }

// MQTT publish condition
if (WiFi.isConnected() && client.connected()) {
    if (stateChanged || millis() - lastMqttPublishTime >= 60000) { ... }
}
```

---

### 3. `readEnvironmentalSensor()` - Get Sensor Data

**Purpose**: Read temperature, humidity, and pressure from BME280

**Called**: Every 3 seconds (in `loop()`)

**Parameters**: None

**Return**: void

**Modifies**:
```cpp
currentSensorData.temperature  // °C
currentSensorData.humidity     // %
currentSensorData.pressure     // hPa
```

**Implementation**:
```cpp
void readEnvironmentalSensor() {
    currentSensorData.temperature = bme280.readTemperature();
    currentSensorData.humidity = bme280.readHumidity();
    currentSensorData.pressure = bme280.readPressure() / 100.0F;
    
    // Log to serial
    Serial.printf("[SENSOR] Temp: %.1f°C | Humidity: %.1f%% | Pressure: %.1f hPa\n",
                  currentSensorData.temperature,
                  currentSensorData.humidity,
                  currentSensorData.pressure);
}
```

**Important Notes**:
- Pressure is converted from Pa to hPa: `value / 100.0`
- Sensor is non-blocking, safe to call frequently
- Values are immediately available after call

**Example Output**:
```
[SENSOR] Temp: 25.5°C | Humidity: 65.3% | Pressure: 1013.2 hPa
```

---

### 4. `updateDisplay()` - Refresh 7-Segment Display

**Purpose**: Cycle through temperature → pressure → humidity display

**Called**: Every 3 seconds (in `loop()`) after sensor read

**Parameters**: None

**Return**: void

**Display Cycle** (every 2 seconds):
```
Mode 0: Temperature (25°C → shows 0025)
   ↓ (2 seconds)
Mode 1: Pressure (1013 hPa → shows 1013)
   ↓ (2 seconds)
Mode 2: Humidity (65% → shows 0065)
   ↓ (2 seconds, repeat)
```

**Implementation**:
```cpp
void updateDisplay() {
    if (millis() - systemState.lastDisplayChangeTime >= DISPLAY_CHANGE_INTERVAL) {
        systemState.lastDisplayChangeTime = millis();
        systemState.displayMode = (systemState.displayMode + 1) % 3;  // Cycle 0→1→2→0
    }
    
    int displayValue = 0;
    if (systemState.displayMode == 0) {
        displayValue = (int)currentSensorData.temperature;
    } else if (systemState.displayMode == 1) {
        displayValue = (int)currentSensorData.pressure;
    } else {
        displayValue = (int)currentSensorData.humidity;
    }
    
    display.showNumber(displayValue, false);
}
```

**Serial Output Example**:
```
[DISPLAY] TEMP: 25
[DISPLAY] PRES: 1013
[DISPLAY] HUM : 65
```

---

### 5. `checkSafetyLimits()` - Temperature Alert Detection

**Purpose**: Monitor temperature and trigger alerts when threshold exceeded

**Called**: Every 3 seconds (after sensor read)

**Parameters**: None

**Return**: void

**Temperature Threshold**: `28.0°C` (configurable via `TEMP_ALERT_THRESHOLD`)

**Alert Behavior**:
```
Temperature ≤ 28.0°C
├─ isAlertActive = false
├─ Green LED ON, Red LED OFF
└─ Buzzer silent

Temperature > 28.0°C
├─ isAlertActive = true
├─ Red LED ON, Green LED OFF
├─ Buzzer pulses every 2 seconds
└─ MQTT publishes immediately
```

**Implementation**:
```cpp
void checkSafetyLimits() {
    bool wasAlertActive = systemState.isAlertActive;
    
    if (currentSensorData.temperature > TEMP_ALERT_THRESHOLD) {
        systemState.isAlertActive = true;
        if (!wasAlertActive) {
            Serial.println("[ALERT] Temperature threshold exceeded!");
        }
    } else {
        systemState.isAlertActive = false;
        if (wasAlertActive) {
            Serial.println("[OK] Temperature returned to normal");
        }
    }
}
```

**Customization**:
```cpp
const float TEMP_ALERT_THRESHOLD = 28.0;  // Change this value
```

---

### 6. `checkEmergencyButton()` - SOS Button Handler

**Purpose**: Detect emergency button press and trigger emergency mode

**Called**: Continuously in `loop()`

**Parameters**: None

**Return**: void

**Button Details**:
- Pin: GPIO 35
- Logic: Active LOW (press connects to GND)
- Debounce: 50ms
- Pull-up: Internal (INPUT_PULLUP)

**Behavior**:
```
Button NOT Pressed (HIGH)
├─ isEmergencyActive = false
├─ Green LED ON
└─ No buzzer (unless temp alert)

Button Pressed (LOW) - Edge Detection
├─ isEmergencyActive = true
├─ Red LED ON
├─ Buzzer sounds (500ms)
├─ Serial logs: "!!! [EMERGENCY] SOS BUTTON PRESSED !!!"
└─ MQTT publishes immediately

Button Released (HIGH)
├─ isEmergencyActive = false
├─ Green LED ON
└─ Buzzer stops
```

**State Machine**:
```
        Button HIGH
           ↓
Not Pressed ←→ Pressed
    ↑           ↓
    └─ Button LOW
```

**Implementation**:
```cpp
void checkEmergencyButton() {
    int buttonState = digitalRead(BUTTON_PIN);  // Read current state
    
    // Debounce check
    if (millis() - systemState.lastButtonDebounceTime < DEBOUNCE_DELAY) {
        return;
    }
    
    // Rising edge (LOW to HIGH transition)
    if (buttonState == LOW && !systemState.buttonWasPressedLastCycle) {
        systemState.isEmergencyActive = true;
        systemState.buttonWasPressedLastCycle = true;
        soundBuzzer(500);  // Beep for 500ms
    }
    // Falling edge (HIGH to LOW transition)
    else if (buttonState == HIGH && systemState.buttonWasPressedLastCycle) {
        systemState.isEmergencyActive = false;
        systemState.buttonWasPressedLastCycle = false;
    }
}
```

**Serial Output**:
```
[DEBUG] Button raw state: LOW (pressed) | Emergency active: YES | Temperature: 25.5
!!! [EMERGENCY] SOS BUTTON PRESSED !!!
[INFO] Button released, emergency mode OFF
```

---

### 7. `checkShutdownButton()` - Alarm Shutdown Handler

**Purpose**: Silence all buzzer/alert activity via physical button

**Called**: Continuously in `loop()`

**Parameters**: None

**Return**: void

**Button Details**:
- Pin: GPIO 34
- Logic: Active LOW (press connects to GND)
- Debounce: 50ms
- Use Case: Quick way to silence active alarms

**Behavior**:
```
Button Pressed (LOW)
├─ isEmergencyActive = false
├─ isAlertActive = false
├─ Buzzer OFF (digitalWrite(BUZZER_PIN, LOW))
├─ Red LED OFF
├─ Green LED ON
└─ Serial: "[OK] All alarms disabled - System reset to normal"

Button Released
└─ No further action
```

**Implementation**:
```cpp
void checkShutdownButton() {
    int shutdownButtonState = digitalRead(BUTTON_SHUTDOWN_PIN);
    
    if (millis() - lastShutdownDebounceTime < DEBOUNCE_DELAY) {
        return;
    }
    
    if (shutdownButtonState == LOW && !shutdownWasPressedLastCycle) {
        shutdownWasPressedLastCycle = true;
        
        // Disable all alarms
        systemState.isEmergencyActive = false;
        systemState.isAlertActive = false;
        digitalWrite(BUZZER_PIN, LOW);
        systemState.buzzerActive = false;
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, HIGH);
        
        Serial.println("[OK] All alarms disabled - System reset to normal");
    }
}
```

---

### 8. `updateAlertSystem()` - LED & Buzzer Control

**Purpose**: Manage visual and audio indicators based on system state

**Called**: Every 10ms (in `loop()`)

**Parameters**: None

**Return**: void

**State Mapping**:
```
isAlertActive OR isEmergencyActive
├─ Red LED ON, Green LED OFF
├─ Buzzer pulse every 2 seconds (if not already sounding)
└─ MQTT alert status: "ALERT"

Normal (both false)
├─ Green LED ON, Red LED OFF
├─ Buzzer silent
└─ MQTT alert status: "NORMAL"
```

**Implementation**:
```cpp
void updateAlertSystem() {
    if (systemState.isAlertActive || systemState.isEmergencyActive) {
        // Red alert
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, HIGH);
        
        // Pulse buzzer every 2 seconds
        if (millis() - systemState.lastBuzzerAlertTime >= 2000) {
            systemState.lastBuzzerAlertTime = millis();
            soundBuzzer(BUZZER_ALERT_DURATION);  // 1000ms
        }
    } else {
        // Normal operation
        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, LOW);
    }
}
```

**LED Status**:
| State | Green LED | Red LED |
|-------|-----------|---------|
| Normal | ON | OFF |
| Alert | OFF | ON |
| Emergency | OFF | ON |

---

### 9. `soundBuzzer()` - Generate Alarm Tone

**Purpose**: Non-blocking buzzer trigger with automatic timeout

**Called**: From `checkEmergencyButton()`, `updateAlertSystem()`, or other alerts

**Parameters**:
```cpp
int duration  // Duration in milliseconds (e.g., 500ms)
```

**Return**: void

**Implementation**:
```cpp
void soundBuzzer(int duration) {
    systemState.buzzerActive = true;
    systemState.buzzerStartTime = millis();
    systemState.buzzerDuration = duration;
    digitalWrite(BUZZER_PIN, HIGH);  // Turn on buzzer
    
    Serial.printf("[BUZZER] Sounding for %dms\n", duration);
}
```

**How It Works (Non-blocking)**:
```
Call soundBuzzer(500)
├─ Sets systemState.buzzerActive = true
├─ Records start time
├─ Turns GPIO 26 HIGH
└─ Returns immediately

Later in loop():
├─ Check if (buzzerActive && elapsed > duration)
├─ If yes: GPIO 26 LOW, buzzerActive = false
└─ If no: wait and check again
```

**Usage Examples**:
```cpp
soundBuzzer(500);    // Beep for 500ms (emergency button)
soundBuzzer(1000);   // Beep for 1 second (temperature alert)
soundBuzzer(100);    // Quick chirp (notification)
```

**Serial Output**:
```
[BUZZER] Sounding for 500ms
[BUZZER] Sounding for 1000ms
```

---

### 10. `publishToMQTT()` - Send Regular Data

**Purpose**: Publish sensor data and status to MQTT broker

**Called**: Every 60 seconds (or immediately on state change)

**Parameters**: None

**Return**: void

**JSON Payload**:
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

**Implementation**:
```cpp
void publishToMQTT() {
    StaticJsonDocument<256> doc;
    
    doc["device_id"] = "SmartHospitalMonitor_01";
    doc["temperature"] = currentSensorData.temperature;
    doc["humidity"] = currentSensorData.humidity;
    doc["pressure"] = currentSensorData.pressure;
    doc["alert_status"] = systemState.isAlertActive ? "ALERT" : "NORMAL";
    doc["emergency_status"] = systemState.isEmergencyActive ? "EMERGENCY" : "OK";
    doc["timestamp"] = millis();
    
    char buffer[256];
    serializeJson(doc, buffer);
    
    if (client.publish(mqtt_topic, buffer)) {
        Serial.printf("[MQTT] Published: %s\n", buffer);
    } else {
        Serial.println("[ERROR] MQTT publish failed!");
    }
}
```

**Publishing Triggers**:
```
Publishing happens when:
├─ 60 seconds elapsed since last publish (normal)
├─ Temperature alert status changes
├─ Emergency button status changes
└─ State change = immediate publish + log
```

**Serial Output**:
```
[MQTT] Published: {"device_id":"SmartHospitalMonitor_01",...}
[ERROR] MQTT publish failed!
```

---

### 11. `publishAlertToMQTT()` - Send Alert (Optional)

**Purpose**: Publish urgent alert with additional context (not currently called)

**Called**: Manual integration available

**Parameters**: None

**Return**: void

**Alert Payload**:
```json
{
  "device_id": "SmartHospitalMonitor_01",
  "alert_type": "EMERGENCY_SOS",
  "temperature": 25.5,
  "humidity": 65.3,
  "pressure": 1013.2,
  "timestamp": 123456789
}
```

---

### 12. `reconnectMQTT()` - Establish MQTT Connection

**Purpose**: Connect to MQTT broker with credential authentication

**Called**: During `setup()` and from `loop()` if disconnected

**Parameters**: None

**Return**: void

**Connection Flow**:
```
1. Check if WiFi is connected
2. Generate unique client ID: "SmartHospitalMonitor_xxxx"
3. Attempt MQTT connect with credentials
   ├─ Success: Log "[OK] MQTT connected!"
   └─ Failure: Log error code, retry after 5 seconds
4. Loop until connected (blocking!)
```

**Implementation**:
```cpp
void reconnectMQTT() {
    while (!client.connected() && WiFi.isConnected()) {
        Serial.printf("[MQTT] Attempting connection to %s\n", mqtt_server);
        
        String clientId = "SmartHospitalMonitor_";
        clientId += String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println("[OK] MQTT connected!");
        } else {
            Serial.printf("[ERROR] MQTT failed, rc=%d Retrying in 5s...\n", 
                         client.state());
            delay(5000);  // Wait 5 seconds before retry
        }
    }
}
```

**MQTT States**:
| State | Meaning |
|-------|---------|
| 0 | Connected |
| -1 | Bad protocol |
| -2 | Bad client ID |
| -3 | Unavailable |
| -4 | Bad credentials |
| -5 | Unauthorized |

---

### 13. WiFi Callbacks

#### `saveConfigCallback()` - Configuration Saved

```cpp
void saveConfigCallback() {
    Serial.println("\n[CALLBACK] Configuration saved via Web Portal!");
    shouldSaveConfig = true;
    connectedViaPortal = true;
}
```

**Purpose**: Called when user saves WiFi config in portal

**Triggers**: When user clicks "Save" in setup portal

---

#### `configModeCallback()` - Portal Started

```cpp
void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("[STATE] Failed to connect automatically");
    Serial.println("[STATE] Starting Configuration Portal...");
    Serial.printf("[INFO] AP Name: %s\n", myWiFiManager->getConfigPortalSSID());
    Serial.printf("[INFO] AP IP: %s\n", WiFi.softAPIP());
}
```

**Purpose**: Called when auto-connect fails and portal starts

**Access**: Connect to `HospitalBoard-Setup` / `setup1234`

---

## Data Structures

### SensorData Structure
```cpp
struct SensorData {
    float temperature;      // °C (0-100)
    float humidity;        // % (0-100)
    float pressure;        // hPa (800-1200)
}
currentSensorData = {0.0, 0.0, 0.0};
```

### SystemState Structure
```cpp
struct SystemState {
    bool isAlertActive;              // Temperature > threshold
    bool isEmergencyActive;          // Emergency button pressed
    bool buttonWasPressedLastCycle;  // Previous cycle state
    unsigned long lastSensorReadTime;
    unsigned long lastMqttPublishTime;
    unsigned long lastBuzzerAlertTime;
    unsigned long lastButtonDebounceTime;
    unsigned long buzzerStartTime;
    unsigned long buzzerDuration;
    bool buzzerActive;
    int displayMode;                 // 0=temp, 1=pressure, 2=humidity
    unsigned long lastDisplayChangeTime;
    bool lastAlertState;             // Track state changes
    bool lastEmergencyState;
}
systemState;
```

---

## Configuration & Customization

### Pin Configuration

Edit in `main.cpp`:
```cpp
// Display
const int PIN_CLK = 18;           // TM1637 Clock
const int PIN_DIO = 5;            // TM1637 Data

// Status Indicators
const int GREEN_LED_PIN = 32;
const int RED_LED_PIN = 33;
const int BUZZER_PIN = 26;

// Input
const int BUTTON_PIN = 35;        // Emergency SOS
const int BUTTON_SHUTDOWN_PIN = 34;  // Shutdown
```

### Timing Configuration

```cpp
const unsigned long SENSOR_READ_INTERVAL = 3000;        // 3 seconds
const unsigned long DISPLAY_CHANGE_INTERVAL = 2000;     // 2 seconds
const unsigned long MQTT_PUBLISH_INTERVAL = 60000;      // 60 seconds
const unsigned long BUZZER_ALERT_DURATION = 1000;       // 1 second
```

### Alert Threshold

```cpp
const float TEMP_ALERT_THRESHOLD = 28.0;  // Change to desired temperature
```

### MQTT Configuration

```cpp
const char* mqtt_server = "broker.address.com";
const int mqtt_port = 8883;
const char* mqtt_user = "your_username";
const char* mqtt_pass = "your_password";
const char* mqtt_topic = "hospital/sensor/data";
```

### Brightness Adjustment

In `setup()`:
```cpp
display.setBrightness(4);  // Range: 0-7 (7 = brightest)
```

---

## Step-by-Step Workflow

### First Boot

1. **Connect to `HospitalBoard-Setup` WiFi** (password: `setup1234`)
2. **Open browser**: `192.168.4.1`
3. **Select your WiFi network** and enter password
4. **Click Save** → Device connects and reboots
5. **Device displays `8888`** → Ready for operation

### Normal Operation

1. **Sensor Reading** (every 3 seconds)
   - Read temperature, humidity, pressure
   - Store in `currentSensorData`

2. **Display Update** (every 2 seconds rotation)
   - Show temperature → pressure → humidity
   - Cycle repeats

3. **Safety Check** (every 3 seconds)
   - Compare temperature to 28.0°C threshold
   - If exceeded: set `isAlertActive = true`

4. **MQTT Publishing** (every 60 seconds + immediate on changes)
   - Send JSON with all sensor data
   - Include alert/emergency status

5. **Alert System** (continuous monitoring)
   - If alert/emergency: Red LED ON, Green LED OFF
   - Pulse buzzer every 2 seconds

### Emergency Response

1. **Press Emergency Button**
   - `isEmergencyActive` becomes `true`
   - Red LED activates
   - Buzzer sounds for 500ms
   - MQTT publishes immediately

2. **Temperature Alert Triggers** (if temp > 28°C)
   - `isAlertActive` becomes `true`
   - Red LED activates  
   - Buzzer pulses every 2 seconds
   - MQTT publishes immediately

3. **Press Shutdown Button** (Pin 34)
   - All alerts disabled
   - Green LED ON
   - Buzzer OFF
   - System returns to normal monitoring

---

## Advanced Usage

### Custom MQTT Publishing

Add this to `loop()` after MQTT publish section:
```cpp
// Custom topic publish
if (systemState.isAlertActive) {
    char alertBuffer[100];
    sprintf(alertBuffer, "ALERT: Temp %f", currentSensorData.temperature);
    client.publish("hospital/alerts", alertBuffer);
}
```

### Add Temperature Alerts at Different Levels

Modify `checkSafetyLimits()`:
```cpp
void checkSafetyLimits() {
    if (currentSensorData.temperature > 35.0) {
        systemState.isAlertActive = true;
        // Critical alert
        soundBuzzer(500);
    } else if (currentSensorData.temperature > 30.0) {
        systemState.isAlertActive = true;
        // Warning
    } else {
        systemState.isAlertActive = false;
    }
}
```

### Change Display Cycle

Modify timing in configuration:
```cpp
const unsigned long DISPLAY_CHANGE_INTERVAL = 5000;  // Change every 5 seconds
```

Or change cycle in `updateDisplay()`:
```cpp
// Only show temperature
displayValue = (int)currentSensorData.temperature;
```

### Continuous Temperature Display

Replace entire `updateDisplay()`:
```cpp
void updateDisplay() {
    // Always show temperature
    int tempDisplay = (int)currentSensorData.temperature;
    display.showNumber(tempDisplay, false);
}
```

### Add Custom Logging Function

```cpp
void logCurrentStatus() {
    Serial.printf("[STATUS] Temp: %.1f°C, Humidity: %.1f%%, Alert: %s\n",
                  currentSensorData.temperature,
                  currentSensorData.humidity,
                  systemState.isAlertActive ? "YES" : "NO");
}

// Call from loop() every few seconds
static unsigned long lastLog = 0;
if (millis() - lastLog > 5000) {
    lastLog = millis();
    logCurrentStatus();
}
```

---

## Troubleshooting Function Issues

### Sensor Always Shows 1111
- **Function**: `setup()` → BME280 initialization
- **Fix**: Check I2C wiring (GPIO 21 SDA, GPIO 22 SCL)
- **Alternative**: Try both addresses (0x76, 0x77)

### Buzzer Not Working
- **Function**: `soundBuzzer()` and `updateAlertSystem()`
- **Check**: 
  - Is buzzer connected to GPIO 26?
  - Is it active LOW or HIGH?
  - Test with: `digitalWrite(26, HIGH);`

### Button Not Responding
- **Function**: `checkEmergencyButton()` / `checkShutdownButton()`
- **Check**:
  - Button connected to correct GPIO (35 and 34)?
  - Pull-up resistor present?
  - Serial monitor shows button state changes?

### WiFi Connection Fails
- **Function**: `setup()` WiFi initialization
- **Fix**:
  - Look for `HospitalBoard-Setup` AP
  - Visit `192.168.4.1`
  - Save your WiFi credentials

### MQTT Not Publishing
- **Function**: `publishToMQTT()` and `reconnectMQTT()`
- **Check**:
  - WiFi connected? (Serial shows IP address)
  - MQTT broker reachable? (Try online MQTT client)
  - Credentials correct? (Username/password)
  - Port 8883 accessible? (Check firewall)

---

## Memory Usage Tips

- **JSON Document**: Fixed at 256 bytes (sufficient for data)
- **Serial Buffer**: 115200 baud supports real-time logging
- **MQTT Buffer**: ~150 bytes per message
- **Total RAM Used**: ~40KB (ESP32 has 520KB available)

---

## Performance Metrics

| Operation | Frequency | Time | Impact |
|-----------|-----------|------|--------|
| Sensor Read | 3 sec | 10ms | Low |
| Display Update | 2 sec | 5ms | Low |
| MQTT Publish | 60 sec | 50ms | Low |
| Button Check | Continuous | <1ms | Negligible |
| WiFi Status | 10 sec | 5ms | Low |

---

## Support & Resources

- **BME280 Library**: Adafruit Unified Sensor
- **MQTT Library**: PubSubClient (knolleary)
- **Display Library**: NextTM1637
- **WiFi Library**: WiFiManager (tzapu)

---

**Last Updated**: April 14, 2026  
**Firmware Version**: Smart Hospital Monitor v1.0  
**ESP32 Board**: UPesy WROOM
