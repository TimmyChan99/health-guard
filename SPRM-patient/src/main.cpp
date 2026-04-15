#include <Arduino.h>
#include <NextBME.h>
#include <NextTM1637.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <NextMPU6050.h>

// Declare Network and MQTT 
WiFiClientSecure network;
PubSubClient mqtt(network);

struct Topics {
  const char* vitals = "patient/vitals";
  const char* fall = "patient/fall";
  const char* call = "patient/call";
};

Topics mqttTopics;

// Declare BME
Adafruit_BME280 bmeSensor;
const u_int8_t BME_ADDRESS = 0x77;

// Declare 7 segment display
const int PIN_CLK = 18;
const int PIN_DIO = 5;
NextTM1637 display(PIN_CLK, PIN_DIO);

// Declare MPU
const int SDA_PIN = 21; // GPIO 21
const int SCL_PIN = 22; // GPIO 22
NextMPU6050 mpu;

// Millis 
unsigned long lastSwitchTime = 0;
int currentValueIndex = 0;
const int displayInterval = 2000;

// PINs
const int SOS_BTN_PIN = 34;
int readSOSBtn;
const int BUZZER_PIN = 26;
const int ALERT_LED_PIN = 33;


void connectMqtt() {
    Serial.println("Connecting to MQTT...");

    if (!mqtt.connect("ESP32-Server-Monitor", MQTT_USER, MQTT_PASS)) {
        Serial.print("MQTT connection failed: ");
        Serial.println(mqtt.state());
        delay(2000);
    } else {
        Serial.println("MQTT connected");
    }  
}

void connectWifi() {
    Serial.println("Connecting to WiFi...");

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.println('.');
        delay(500);
    }
    
    Serial.println("Wifi connected");
    network.setInsecure();
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void startSensors() {
    if (!bmeSensor.begin(BME_ADDRESS)) {
        Serial.println("No sensors found at address 0x77. Check wiring/addresses.");
        while (1); 
    }

    Serial.println("BME280 found!");

    Wire.begin(SDA_PIN, SCL_PIN);

    if (!mpu.begin()) {
      Serial.println("Failed to find MPU6050 sensor!");
      while (1);
    }
  
    Serial.println("MPU6050 Found!");

    display.begin();
    display.setBrightness(4);
}

void sendJsonMQTT(const char* alerType) {
    StaticJsonDocument<200> doc;

    doc["deviceId"] = "MakerBoard_01";
    doc["temperature"] = bmeSensor.readTemperature();
    doc["pressure"] = bmeSensor.readPressure() / 100.0;
    doc["humidity"] = bmeSensor.readHumidity();
    doc["alert"] = alerType;

    char buffer[256];
    serializeJson(doc, buffer);

    mqtt.publish(alerType, buffer);
    Serial.print("Published: ");
    Serial.println(buffer);
}

void patientCall() {
  digitalWrite(ALERT_LED_PIN, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(ALERT_LED_PIN, LOW);
}

void btnControl() {
    readSOSBtn = digitalRead(SOS_BTN_PIN);

    if (readSOSBtn == LOW) {
      patientCall();
      sendJsonMQTT(mqttTopics.call);

      delay(100);
    }
}

// ── Thresholds ──────────────────────────────────────────────────────────────
// Free-fall: total acceleration drops below this value (in g)
const float FREEFALL_THRESHOLD = 0.5;

// Impact: total acceleration exceeds this value (in g)
const float IMPACT_THRESHOLD = 2.5;

// After impact, if the person is still lying down the Z-axis will be near 0
// (the sensor is now horizontal instead of vertical).
// If Z > this value, the person likely recovered or was just a bump.
const float POSTURE_Z_THRESHOLD = 0.7;

// Maximum time (ms) between free-fall detection and impact detection.
// If impact doesn't follow free-fall within this window, reset.
const unsigned long FALL_WINDOW_MS = 500;

// Minimum time (ms) to keep the fall flag active before resetting.
// Prevents re-triggering immediately.
const unsigned long FALL_COOLDOWN_MS = 5000;

// ── State machine ───────────────────────────────────────────────────────────
enum FallState {
  IDLE,
  FREEFALL_DETECTED,
  FALL_CONFIRMED
};

FallState fallState = IDLE;
unsigned long freefallTimestamp = 0;
unsigned long fallConfirmedTimestamp = 0;
bool fallAlertSent = false;

// ── Helper ──────────────────────────────────────────────────────────────────
float totalAcceleration(float ax, float ay, float az) {
  return sqrt(ax * ax + ay * ay + az * az);
}

void triggerFallAlert() {
  Serial.println(">>> FALL DETECTED — publishing to MQTT patient/fall <<<");
  sendJsonMQTT(mqttTopics.fall);
}

void fallDectection() {
  
  float ax = mpu.getAccelX();
  float ay = mpu.getAccelY();
  float az = mpu.getAccelZ();

  float gx = mpu.getGyroX();
  float gy = mpu.getGyroY();
  float gz = mpu.getGyroZ();

  float acc = totalAcceleration(ax, ay, az);
  unsigned long now = millis();

  // ── STATE MACHINE ──────────────────────────────────────────────────────

  switch (fallState) {

    case IDLE:
      // Step 1: detect the free-fall phase
      if (acc < FREEFALL_THRESHOLD) {
        fallState = FREEFALL_DETECTED;
        freefallTimestamp = now;
        Serial.println("[FALL] Free-fall phase detected");
      }
      break;

    case FREEFALL_DETECTED:
      // Step 2: wait for impact within the time window
      if (now - freefallTimestamp > FALL_WINDOW_MS) {
        // Too slow — not a fall, reset
        Serial.println("[FALL] No impact after free-fall — reset");
        fallState = IDLE;
        break;
      }

      if (acc > IMPACT_THRESHOLD) {
        // Impact detected — now check posture
        // If az is low, the person is now lying flat (sensor horizontal)
        if (abs(az) < POSTURE_Z_THRESHOLD) {
          fallState = FALL_CONFIRMED;
          fallConfirmedTimestamp = now;
          fallAlertSent = false;
          Serial.println("[FALL] Impact + horizontal posture -> FALL CONFIRMED");
        } else {
          // High impact but person is still upright — could be a bump/stumble
          Serial.println("[FALL] Impact detected but posture normal — ignoring");
          fallState = IDLE;
        }
      }
      break;

    case FALL_CONFIRMED:
      // Send the alert only once
      if (!fallAlertSent) {
        triggerFallAlert();
        fallAlertSent = true;
      }

      // After cooldown, return to monitoring
      if (now - fallConfirmedTimestamp > FALL_COOLDOWN_MS) {
        Serial.println("[FALL] Cooldown complete — resuming monitoring");
        fallState = IDLE;
      }
      break;
  }

  // // ── Debug output ────────────────────────────────────────────────────────
  // Serial.print("acc_total=");
  // Serial.print(acc, 3);
  // Serial.print("g  az=");
  // Serial.print(az, 3);
  // Serial.print("g  state=");
  // Serial.println(fallState == IDLE ? "IDLE" : fallState == FREEFALL_DETECTED ? "FREEFALL" : "CONFIRMED");

  // delay(20);
}

void updateDisplay() {
  unsigned long currentTime = millis();

    if (currentTime - lastSwitchTime >= displayInterval) {
        lastSwitchTime = currentTime;
        
        double val = 0;
        
        // Cycle through 0: Temp, 1: Pressure
        switch (currentValueIndex) {
            case 0:
                val = bmeSensor.readTemperature();
                display.showNumber(val * 100, false);
                display.setColon(true);
                break;
            case 1:
                val = bmeSensor.readPressure() / 100.0;
                display.showNumber( (int)val, false);
                display.setColon(false);
                break;
        }

        // Loop back to 0
        currentValueIndex = (currentValueIndex + 1) % 2;
    }
}

void setup() {
    Serial.begin(115200);

    while(Serial.available() == 0) {
        delay(10);
    }

    connectWifi();
    startSensors();

    pinMode(SOS_BTN_PIN, INPUT);
    pinMode(ALERT_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
}

unsigned long previousMillis = 0;
const long interval = 30000;

void loop() {
    if (!mqtt.connected()) {
        connectMqtt();
    }

    mqtt.loop();

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
      sendJsonMQTT(mqttTopics.vitals);
      previousMillis = currentMillis;
    }

    updateDisplay();

    fallDectection();
    btnControl();
}
