#include <Arduino.h>
#include <NextTM1637.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//--- MQTT Configuration ---
const char* mqtt_server = "e16a2ae3d3034163bb42dceb064fe375.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "FUSION_AI";
const char* mqtt_pass = "Aa12345678";
const char* mqtt_topic = "hospital/sensor/data";

//--- Safety Limits ---
const float TEMP_ALERT_THRESHOLD = 28.0;  // Temperature threshold that triggers alert

//--- Pin Definitions ---
// Display
const int PIN_CLK = 18;        // TM1637 Clock
const int PIN_DIO = 5;         // TM1637 Data

// Status Indicators
const int GREEN_LED_PIN = 32;  // Green LED (normal status)
const int RED_LED_PIN = 33;    // Red LED (alert status)
const int BUZZER_PIN = 26;     // Buzzer/Audio alarm

// Input
const int BUTTON_PIN = 35;     // Emergency SOS button
const int BUTTON_SHUTDOWN_PIN = 34;  // Buzzer shutdown button (for testing)

// BME280 uses I2C (GPIO 21 SDA, GPIO 22 SCL on ESP32)
#define I2C_SDA 21
#define I2C_SCL 22

//--- Timing Configuration ---
const unsigned long SENSOR_READ_INTERVAL = 3000;      // Read sensor every 3 seconds
const unsigned long DISPLAY_CHANGE_INTERVAL = 2000;   // Change display every 2 seconds
const unsigned long MQTT_PUBLISH_INTERVAL = 60000;    // Publish to MQTT every 60 seconds (1 minute)
const unsigned long BUZZER_ALERT_DURATION = 1000;     // Buzzer alert duration (ms)

//--- Global Objects ---
NextTM1637 display(PIN_CLK, PIN_DIO);
Adafruit_BME280 bme280;
WiFiClientSecure espClient;
PubSubClient client(espClient);

//--- Global Variables ---
struct SensorData {
    float temperature;
    float humidity;
    float pressure;
} currentSensorData = {0.0, 0.0, 0.0};

struct SystemState {
    bool isAlertActive = false;           // Temperature exceeds threshold
    bool isEmergencyActive = false;       // Emergency button pressed
    bool buttonWasPressedLastCycle = false;
    unsigned long lastSensorReadTime = 0;
    unsigned long lastMqttPublishTime = 0;
    unsigned long lastBuzzerAlertTime = 0;
    unsigned long lastButtonDebounceTime = 0;
    unsigned long buzzerStartTime = 0;
    unsigned long buzzerDuration = 0;
    bool buzzerActive = false;
    int displayMode = 0;                  // 0=temp, 1=pressure, 2=humidity
    unsigned long lastDisplayChangeTime = 0;
    bool lastAlertState = false;          // Track previous alert state for immediate publish
    bool lastEmergencyState = false;      // Track previous emergency state for immediate publish
} systemState;

//--- Function Declarations ---
void reconnectMQTT();
void readEnvironmentalSensor();
void updateDisplay();
void checkSafetyLimits();
void checkEmergencyButton();
void checkShutdownButton();
void updateAlertSystem();
void soundBuzzer(int duration);
void publishToMQTT();
void publishAlertToMQTT();

//--- WiFi State Variables ---
bool shouldSaveConfig = false;
bool portalTriggered = false;
bool connectedViaPortal = false;

// Callback when configuration is saved via portal
void saveConfigCallback() {
  Serial.println("\n[CALLBACK] Configuration saved via Web Portal!");
  shouldSaveConfig = true;
  connectedViaPortal = true;
}

// Callback when portal starts (meaning auto-connect failed)
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("\n[STATE] Failed to connect automatically using saved credentials");
  Serial.println("[STATE] Starting Configuration Portal...");
  Serial.print("[INFO] AP Name: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("[INFO] AP IP: ");
  Serial.println(WiFi.softAPIP());
  portalTriggered = true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== Smart Hospital Monitor Starting ===");
    
    // Initialize WiFi Manager
    Serial.println("[STATE] Checking for saved WiFi credentials...");
    WiFiManager wm;
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(180);
    
    const char* apName = "HospitalBoard-Setup";
    const char* apPassword = "setup1234";
    
    if (WiFi.SSID() != "") {
        Serial.print("[INFO] Found saved network: ");
        Serial.println(WiFi.SSID());
        Serial.println("[STATE] Attempting AUTO-CONNECT with saved credentials...");
    } else {
        Serial.println("[INFO] No saved credentials found");
    }
    
    bool wifiRes = wm.autoConnect(apName, apPassword);
    
    if (wifiRes) {
        Serial.println("\n===================================");
        Serial.println("[SUCCESS] CONNECTED TO WIFI!");
        Serial.println("===================================");
        Serial.print("[NETWORK] SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("[NETWORK] IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("[NETWORK] Signal Strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    } else {
        Serial.println("\n===================================");
        Serial.println("[WARN] WiFi connection timeout!");
        Serial.println("===================================");
        Serial.println("[STATE] Continuing without WiFi...");
    }
    
    // Initialize pins
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUTTON_SHUTDOWN_PIN, INPUT_PULLUP);
    
    // Set initial LED state (green = normal)
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    
    // Initialize display
    Serial.println("[INIT] Initializing TM1637 Display...");
    display.begin();
    display.setBrightness(4);
    display.showNumber(0, false);
    
    // Initialize BME280 sensor via I2C
    Serial.println("[INIT] Initializing BME280 Sensor...");
    if (!bme280.begin(0x76, &Wire) && !bme280.begin(0x77, &Wire)) {
        Serial.println("[ERROR] BME280 not found at 0x76 or 0x77! Check I2C connections.");
        display.showNumber(1111, false);
        // Continue anyway - use dummy values
    } else {
        Serial.println("[OK] BME280 initialized successfully");
    }
    
    // Configure BME280
    bme280.setSampling(Adafruit_BME280::MODE_NORMAL,
                       Adafruit_BME280::SAMPLING_X2,   // temp
                       Adafruit_BME280::SAMPLING_X2,   // pressure
                       Adafruit_BME280::SAMPLING_X2,   // humidity
                       Adafruit_BME280::FILTER_OFF,
                       Adafruit_BME280::STANDBY_MS_1000);
    
    // Configure TLS for MQTT
    espClient.setInsecure();
    
    // Configure MQTT
    client.setServer(mqtt_server, mqtt_port);
    Serial.println("[OK] MQTT server configured");
    
    // Initial MQTT connection (only if WiFi is connected)
    if (WiFi.isConnected()) {
        reconnectMQTT();
    }
    
    // Display ready message (all segments on)
    display.showNumber(8888, false);
    Serial.println("=== System Ready ===\n");
}

void loop() {
    // Monitor WiFi connection status and ensure MQTT connection
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 10000) {  // Check every 10 seconds
        lastWiFiCheck = millis();
        if (WiFi.isConnected()) {
            Serial.print("[WiFi] Connected | IP: ");
            Serial.print(WiFi.localIP());
            Serial.print(" | RSSI: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
        } else {
            Serial.println("[WiFi] DISCONNECTED");
        }
    }
    
    // Ensure MQTT is connected (only if WiFi is connected)
    if (WiFi.isConnected()) {
        if (!client.connected()) {
            reconnectMQTT();
        }
        client.loop();  // Process MQTT messages
    }
    
    // Handle non-blocking buzzer timeout
    if (systemState.buzzerActive) {
        if (millis() - systemState.buzzerStartTime > systemState.buzzerDuration) {
            digitalWrite(BUZZER_PIN, LOW);
            systemState.buzzerActive = false;
        }
    }
    
    // Read environmental sensor at regular intervals
    if (millis() - systemState.lastSensorReadTime >= SENSOR_READ_INTERVAL) {
        systemState.lastSensorReadTime = millis();
        readEnvironmentalSensor();
        updateDisplay();
        checkSafetyLimits();
    }
    
    // Check emergency button continuously
    checkEmergencyButton();
    
    // Check shutdown button for testing
    checkShutdownButton();
    
    // Update alert system (LEDs, buzzer)
    updateAlertSystem();
    
    // Publish data to MQTT
    if (WiFi.isConnected() && client.connected()) {
        bool stateChanged = (systemState.isAlertActive != systemState.lastAlertState) ||
                           (systemState.isEmergencyActive != systemState.lastEmergencyState);
        
        // Publish immediately if state changes to emergency/alert, otherwise publish every minute
        if (stateChanged || millis() - systemState.lastMqttPublishTime >= MQTT_PUBLISH_INTERVAL) {
            systemState.lastMqttPublishTime = millis();
            systemState.lastAlertState = systemState.isAlertActive;
            systemState.lastEmergencyState = systemState.isEmergencyActive;
            publishToMQTT();
        }
    }
    
    delay(10);  // Small delay to prevent watchdog trigger
}

void readEnvironmentalSensor() {
    // Read from BME280
    currentSensorData.temperature = bme280.readTemperature();
    currentSensorData.humidity = bme280.readHumidity();
    currentSensorData.pressure = bme280.readPressure() / 100.0F;  // Convert Pa to hPa
    
    Serial.print("[SENSOR] Temp: ");
    Serial.print(currentSensorData.temperature);
    Serial.print("°C | Humidity: ");
    Serial.print(currentSensorData.humidity);
    Serial.print("% | Pressure: ");
    Serial.print(currentSensorData.pressure);
    Serial.println(" hPa");
}

void updateDisplay() {
    // Check if it's time to change the display
    if (millis() - systemState.lastDisplayChangeTime >= DISPLAY_CHANGE_INTERVAL) {
        systemState.lastDisplayChangeTime = millis();
        systemState.displayMode = (systemState.displayMode + 1) % 3;  // Cycle: 0->1->2->0
    }
    
    int displayValue = 0;
    const char* label = "";
    
    // Select which value to display
    if (systemState.displayMode == 0) {
        displayValue = (int)currentSensorData.temperature;
        label = "TEMP";
    } else if (systemState.displayMode == 1) {
        displayValue = (int)currentSensorData.pressure;
        label = "PRES";
    } else {
        displayValue = (int)currentSensorData.humidity;
        label = "HUM ";
    }
    
    display.showNumber(displayValue, false);
    Serial.print("[DISPLAY] ");
    Serial.print(label);
    Serial.print(": ");
    Serial.println(displayValue);
}

void checkSafetyLimits() {
    // Check if temperature exceeds alert threshold
    bool wasAlertActive = systemState.isAlertActive;
    
    if (currentSensorData.temperature > TEMP_ALERT_THRESHOLD) {
        systemState.isAlertActive = true;
        if (!wasAlertActive) {
            Serial.println("[ALERT] Temperature threshold exceeded! Activating alarm...");
        }
    } else {
        systemState.isAlertActive = false;
        if (wasAlertActive) {
            Serial.println("[OK] Temperature returned to normal");
        }
    }
}

void checkEmergencyButton() {
    static unsigned long lastDebugPrint = 0;
    const unsigned long DEBOUNCE_DELAY = 50;  // 50ms debounce
    
    int buttonState = digitalRead(BUTTON_PIN);
    
    // Debug: Print button state every 3 seconds
    if (millis() - lastDebugPrint >= 3000) {
        lastDebugPrint = millis();
        Serial.print("[DEBUG] Button raw state: ");
        Serial.print(buttonState == LOW ? "LOW (pressed)" : "HIGH (not pressed)");
        Serial.print(" | Emergency active: ");
        Serial.print(systemState.isEmergencyActive ? "YES" : "NO");
        Serial.print(" | Temperature: ");
        Serial.println(currentSensorData.temperature);
    }
    
    // Debounce: only process if enough time has passed
    if (millis() - systemState.lastButtonDebounceTime < DEBOUNCE_DELAY) {
        return;
    }
    
    // Button is active LOW (pulled to GND when pressed with INPUT_PULLUP)
    if (buttonState == LOW && !systemState.buttonWasPressedLastCycle) {
        // Button just pressed
        systemState.lastButtonDebounceTime = millis();
        systemState.isEmergencyActive = true;
        systemState.buttonWasPressedLastCycle = true;
        Serial.println("\n!!! [EMERGENCY] SOS BUTTON PRESSED !!!\n");
        // Sound buzzer
        soundBuzzer(500);
    } 
    else if (buttonState == HIGH && systemState.buttonWasPressedLastCycle) {
        // Button released
        systemState.lastButtonDebounceTime = millis();
        systemState.buttonWasPressedLastCycle = false;
        Serial.println("[INFO] Button released, emergency mode OFF");
    }
}

void checkShutdownButton() {
    static unsigned long lastDebugPrint = 0;
    static bool shutdownWasPressedLastCycle = false;
    const unsigned long DEBOUNCE_DELAY = 50;  // 50ms debounce
    static unsigned long lastShutdownDebounceTime = 0;
    
    int shutdownButtonState = digitalRead(BUTTON_SHUTDOWN_PIN);
    
    // Debug: Print shutdown button state every 3 seconds
    if (millis() - lastDebugPrint >= 3000) {
        lastDebugPrint = millis();
        Serial.print("[DEBUG] Shutdown Button (Pin 34): ");
        Serial.println(shutdownButtonState == LOW ? "LOW (pressed)" : "HIGH (not pressed)");
    }
    
    // Debounce: only process if enough time has passed
    if (millis() - lastShutdownDebounceTime < DEBOUNCE_DELAY) {
        return;
    }
    
    // Shutdown button is active LOW
    if (shutdownButtonState == LOW && !shutdownWasPressedLastCycle) {
        // Button just pressed - shut down all alarms
        lastShutdownDebounceTime = millis();
        shutdownWasPressedLastCycle = true;
        Serial.println("\n>>> [SHUTDOWN] Buzzer shutdown button pressed <<<\n");
        
        // Turn off everything
        systemState.isEmergencyActive = false;
        systemState.isAlertActive = false;
        digitalWrite(BUZZER_PIN, LOW);
        systemState.buzzerActive = false;
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, HIGH);
        
        Serial.println("[OK] All alarms disabled - System reset to normal");
    } 
    else if (shutdownButtonState == HIGH && shutdownWasPressedLastCycle) {
        // Button released
        lastShutdownDebounceTime = millis();
        shutdownWasPressedLastCycle = false;
    }
}

void updateAlertSystem() {
    // Update LED status
    if (systemState.isAlertActive || systemState.isEmergencyActive) {
        // Alert mode: Red LED on
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, HIGH);
        
        // Pulse buzzer periodically
        if (millis() - systemState.lastBuzzerAlertTime >= 2000) {
            systemState.lastBuzzerAlertTime = millis();
            soundBuzzer(BUZZER_ALERT_DURATION);
        }
    } else {
        // Normal mode: Green LED on
        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, LOW);
        // Don't sound buzzer
    }
}

void soundBuzzer(int duration) {
    // Non-blocking buzzer: just start it
    systemState.buzzerActive = true;
    systemState.buzzerStartTime = millis();
    systemState.buzzerDuration = duration;
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.print("[BUZZER] Sounding for ");
    Serial.print(duration);
    Serial.println("ms");
}

void reconnectMQTT() {
    while (!client.connected() && WiFi.isConnected()) {
        Serial.print("[MQTT] Attempting connection to ");
        Serial.println(mqtt_server);
        
        // Generate unique client ID
        String clientId = "SmartHospitalMonitor_";
        clientId += String(random(0xffff), HEX);
        
        // Attempt connection
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println("[OK] MQTT connected!");
            // Subscribe to potential control topics (if needed in future)
            // client.subscribe("hospital/control");
        } else {
            Serial.print("[ERROR] MQTT failed, rc=");
            Serial.print(client.state());
            Serial.println(" Retrying in 5s...");
            delay(5000);
        }
    }
}

void publishToMQTT() {
    // Create JSON document with sensor data and system status
    StaticJsonDocument<256> doc;
    
    doc["device_id"] = "SmartHospitalMonitor_01";
    doc["temperature"] = currentSensorData.temperature;
    doc["humidity"] = currentSensorData.humidity;
    doc["pressure"] = currentSensorData.pressure;
    doc["alert_status"] = systemState.isAlertActive ? "ALERT" : "NORMAL";
    doc["emergency_status"] = systemState.isEmergencyActive ? "EMERGENCY" : "OK";
    doc["timestamp"] = millis();
    
    // Serialize to string
    char buffer[256];
    serializeJson(doc, buffer);
    
    // Publish to MQTT broker
    if (client.publish(mqtt_topic, buffer)) {
        Serial.print("[MQTT] Published: ");
        Serial.println(buffer);
    } else {
        Serial.println("[ERROR] MQTT publish failed!");
    }
}

void publishAlertToMQTT() {
    // Immediate alert notification
    StaticJsonDocument<256> doc;
    
    doc["device_id"] = "SmartHospitalMonitor_01";
    doc["alert_type"] = systemState.isEmergencyActive ? "EMERGENCY_SOS" : "TEMPERATURE_ALERT";
    doc["temperature"] = currentSensorData.temperature;
    doc["humidity"] = currentSensorData.humidity;
    doc["pressure"] = currentSensorData.pressure;
    doc["timestamp"] = millis();
    
    char buffer[256];
    serializeJson(doc, buffer);
    
    if (client.publish(mqtt_topic, buffer)) {
        Serial.print("[MQTT] Alert Published: ");
        Serial.println(buffer);
    }
}