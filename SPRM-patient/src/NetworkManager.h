#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <PubSubClient.h>
#include <WiFiClientSecure.h>

WiFiClientSecure network;
PubSubClient mqtt(network);

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

    WiFi.softAP("Hospital_Device", "12345678");
    Serial.println("Access Point Started");
    Serial.println(WiFi.softAPIP());


    network.setInsecure();
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

#endif
