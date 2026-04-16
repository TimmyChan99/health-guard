#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>

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

void startMDNS() {
    if (MDNS.begin("patient-monitor")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("mDNS: patient-monitor.local");
    }
}

void startAP() {
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("Hospital_Device2", "12345678");
    Serial.println("AP Started: patient-monitor.local");
    
    startMDNS();
}

void connectWifi() {
    Serial.println("Connecting to WiFi...");
    Serial.println("=========================================");
    Serial.println("WiFi Credentials will be configured via");
    Serial.println("the built-in WiFiManager portal.");
    Serial.println("=========================================");
    Serial.println("\n[INFO] If no saved credentials, portal opens");
    Serial.println("[INFO] Connect to 'Hospital_Device2' AP");
    Serial.println("[INFO] Then open browser to patient-monitor.local");

    WiFiManager wm;
    
    IPAddress apIP(192, 168, 4, 1);
    wm.setAPStaticIPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    wm.setConfigPortalTimeout(180);
    wm.setDebugOutput(true);
    
    bool res = wm.autoConnect("Hospital_Device2", "12345678");
    
    if (!res) {
        Serial.println("\n[FAILED] Could not connect to WiFi");
        Serial.println("[INFO] Starting config portal...");
        wm.startConfigPortal("Hospital_Device2", "12345678");
    }
    
    Serial.print("\nWiFi connected: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    startAP();

    network.setInsecure();
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

#endif
