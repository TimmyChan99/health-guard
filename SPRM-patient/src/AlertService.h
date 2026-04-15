#pragma once
#include <Arduino.h>

#include "AlertService.h"
#include <ArduinoJson.h>

extern PubSubClient mqtt;
extern Adafruit_BME280 bmeSensor;

void sendJsonMQTT(const char* topic, const char* alertType) {
    StaticJsonDocument<200> doc;

    doc["deviceId"] = "MakerBoard_01";
    doc["temperature"] = bmeSensor.readTemperature();
    doc["pressure"] = bmeSensor.readPressure() / 100.0;
    doc["humidity"] = bmeSensor.readHumidity();
    doc["alert"] = alertType;

    char buffer[256];
    serializeJson(doc, buffer);

    mqtt.publish(topic, buffer);

    Serial.print("Published: ");
    Serial.println(buffer);
}

void triggerAlert(const char* topic, const char* alertType) {
    sendJsonMQTT(topic, alertType);
}
