#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <NextBME.h>
#include <NextTM1637.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <NextMPU6050.h>
#include "NetworkManager.h"
#include "WebServerManager.h"
#include "AlertService.h"
#include "Config.h"

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
unsigned long lastAlertMillis = 0;
const long ALERT_COOLDOWN_MS = 30000; // 30 seconds cooldown between alerts

// PINs
const int SOS_BTN_PIN = 34;
int readSOSBtn;
const int BUZZER_PIN = 26;
const int ALERT_LED_PIN = 33;


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

void patientCall() {
  digitalWrite(ALERT_LED_PIN, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(ALERT_LED_PIN, LOW);
}

void sosButton() {
    readSOSBtn = digitalRead(SOS_BTN_PIN);

    if (readSOSBtn == LOW) {
      patientCall();
      sendJsonMQTT(TOPIC_ALERTS, alertType.emergencyButton);

      delay(100);
    }
}


// ── Thresholds ──────────────────────────────────────────────────────────────
// Free-fall: total acceleration drops below this value (in g)
const float FREEFALL_THRESHOLD = 0.4;

// Impact: total acceleration exceeds this value (in g)
const float IMPACT_THRESHOLD = 1.5;

// After impact, if the person is still lying down the Z-axis will be near 0
// (the sensor is now horizontal instead of vertical).
// If Z > this value, the person likely recovered or was just a bump.
const float POSTURE_Z_THRESHOLD = 0.7;

// Maximum time (ms) between free-fall detection and impact detection.
// If impact doesn't follow free-fall within this window, reset.
const unsigned long FALL_WINDOW_MS = 1800;

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
  sendJsonMQTT(TOPIC_ALERTS, alertType.fallDetected);
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
        if (abs(az) > POSTURE_Z_THRESHOLD) {
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
  Serial.print("acc_total=");
  Serial.print(acc, 3);
  Serial.print("g  az=");
  Serial.print(az, 3);
  Serial.print("g  state=");
  Serial.println(fallState == IDLE ? "IDLE" : fallState == FREEFALL_DETECTED ? "FREEFALL" : "CONFIRMED");
  Serial.print("Accel: ");
  Serial.print(ax); Serial.print("g, ");
  Serial.print(ay); Serial.print("g, ");
  Serial.print(az); Serial.println("g");


  Serial.println("=========================================================== \n");

  delay(200);
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
    delay(500);

    connectWifi();
    startSensors();
    initWebServer();

    pinMode(SOS_BTN_PIN, INPUT);
    pinMode(ALERT_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    for (int i = 0; i < 6; i++) {
        digitalWrite(ALERT_LED_PIN, i % 2 == 0 ? HIGH : LOW);
        delay(200);
    }
    digitalWrite(ALERT_LED_PIN, LOW);
    Serial.println("Ready! Open browser to patient-monitor.local");
}

void criticalVitals() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastAlertMillis < ALERT_COOLDOWN_MS) return; // Cooldown active

    float temperature = bmeSensor.readTemperature();
    float pressure = bmeSensor.readPressure() / 100;

    bool alertSent = false;

    if (temperature < tempMin) {
      sendJsonMQTT(TOPIC_ALERTS, alertType.lowTemperature);
      alertSent = true;
    };

    if (temperature > tempMax) {
      sendJsonMQTT(TOPIC_ALERTS, alertType.highTemperature);
      alertSent = true;
    }

    if (pressure > bpSys) {
      sendJsonMQTT(TOPIC_ALERTS, alertType.highPressure);
      alertSent = true;
    };

    if (pressure < bpDia) {
      sendJsonMQTT(TOPIC_ALERTS, alertType.lowPressure);
      alertSent = true;
    }

    if (alertSent) {
      lastAlertMillis = currentMillis;
    }
}

unsigned long previousMillis = 0;
long mqttIntervalSeconds = 1;

void loop() {
    if (!mqtt.connected()) {
        connectMqtt();
    }

    mqtt.loop();

    server.handleClient();

    // Handle Critical cases
    criticalVitals();

    // Handle Normal regular case
    unsigned long currentMillis = millis();

    Serial.printf("Current MQTT interval: %ld seconds (%ld ms)\n", mqttIntervalSeconds, mqttIntervalSeconds * 1000);
    if (currentMillis - previousMillis >= mqttIntervalSeconds * 1000) {
      Serial.println(">>> Sending normal MQTT data <<<");
      sendJsonMQTT(TOPIC_VITALS, alertType.normal);
      previousMillis = currentMillis;
    }

    updateDisplay();
    fallDectection();
    sosButton();
}
