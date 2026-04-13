#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager

// State tracking variables
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
  Serial.println("\n\n===================================");
  Serial.println("ESP32 WiFi Manager - State Monitor");
  Serial.println("===================================");

  WiFiManager wm;

  // Set callbacks for state tracking
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setAPCallback(configModeCallback);

  // Optional: Set timeout
  wm.setConfigPortalTimeout(180);

  const char* apName = "ESP32-Setup";
  const char* apPassword = "setup1234";

  Serial.println("\n[STATE] Checking for saved WiFi credentials...");
  
  // Check if we have saved credentials before attempting
  if (WiFi.SSID() != "") {
    Serial.print("[INFO] Found saved network: ");
    Serial.println(WiFi.SSID());
    Serial.println("[STATE] Attempting AUTO-CONNECT with saved credentials...");
  } else {
    Serial.println("[INFO] No saved credentials found");
    Serial.println("[STATE] Will start Configuration Portal immediately...");
  }

  // Attempt connection
  bool res = wm.autoConnect(apName, apPassword);

  // Analyze results
  if (res) {
    Serial.println("\n===================================");
    Serial.println("[SUCCESS] CONNECTED TO WIFI!");
    Serial.println("===================================");
    
    if (connectedViaPortal) {
      Serial.println("[MODE] Connected via WEB INTERFACE (Portal)");
      Serial.println("[INFO] New credentials saved for future auto-connect");
    } else {
      Serial.println("[MODE] Connected via AUTO-CONNECT (saved credentials)");
      Serial.println("[INFO] Used previously saved WiFi settings");
    }
    
    Serial.print("[NETWORK] SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("[NETWORK] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[NETWORK] Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
  } else {
    Serial.println("\n===================================");
    Serial.println("[FAILED] Connection failed!");
    Serial.println("===================================");
    Serial.println("[STATE] Hit timeout waiting for configuration");
    Serial.println("[ACTION] Restarting ESP32...");
    delay(3000);
    ESP.restart();
  }
}

void loop() {
  static unsigned long lastCheck = 0;
  
  // Monitor connection status in real-time
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    
    if (WiFi.isConnected()) {
      Serial.print("[STATUS] Connected | IP: ");
      Serial.print(WiFi.localIP());
      Serial.print(" | RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("[STATUS] DISCONNECTED - Attempting reconnect...");
    }
  }
}