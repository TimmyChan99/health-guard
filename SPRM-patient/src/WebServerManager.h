#pragma once
#include <WebServer.h> 
#include "Config.h"
#include "AlertService.h"

inline WebServer server(80);

// Thresholds
inline float tempMin = 20.0;
inline float tempMax = 35.0;

inline int bpSys = 120;   // systolic
inline int bpDia = 80;    // diastolic

// ================= HTML UI =================
const char page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Patient Monitor</title>
<style>
body {
  font-family: Arial;
  background: #0f172a;
  color: white;
  text-align: center;
  margin: 0;
}
.container {
  padding: 20px;
}
.card {
  background: #1e293b;
  padding: 20px;
  margin: 15px auto;
  border-radius: 12px;
  width: 90%;
  max-width: 400px;
}
h2 {
  margin-bottom: 10px;
}
input {
  width: 80%;
  padding: 8px;
  margin: 5px;
  border-radius: 8px;
  border: none;
}
button {
  padding: 12px 20px;
  border: none;
  border-radius: 10px;
  background: #22c55e;
  color: white;
  font-size: 16px;
  margin-top: 10px;
}
.alert {
  background: #ef4444;
}
</style>
</head>

<body>
<div class="container">

<div class="card">
  <h2>Temperature</h2>
  <p>Min</p>
  <input type="number" id="tempMin" value="20">
  <p>Max</p>
  <input type="number" id="tempMax" value="35">
  <button onclick="setTemp()">Update</button>
</div>

<div class="card">
  <h2>Blood Pressure</h2>
  <p>Systolic</p>
  <input type="number" id="bpSys" value="120">
  <p>Diastolic</p>
  <input type="number" id="bpDia" value="80">
  <button onclick="setBP()">Update</button>
</div>

<div class="card">
  <h2>Emergency</h2>
  <button class="alert" onclick="triggerAlert()">Send Alert</button>
</div>

</div>

<script>
function setTemp() {
  let min = document.getElementById("tempMin").value;
  let max = document.getElementById("tempMax").value;
  fetch(`/setTemp?min=${min}&max=${max}`);
}

function setBP() {
  let sys = document.getElementById("bpSys").value;
  let dia = document.getElementById("bpDia").value;
  fetch(`/setBP?sys=${sys}&dia=${dia}`);
}

function triggerAlert() {
  fetch("/alert");
}
</script>

</body>
</html>
)rawliteral";

// ================= ROUTES =================

inline void handleRoot() {
    server.send(200, "text/html", page);
}

// Temperature update
inline void handleSetTemp() {
    if (server.hasArg("min")) tempMin = server.arg("min").toFloat();
    if (server.hasArg("max")) tempMax = server.arg("max").toFloat();

    Serial.printf("Temp updated: min=%.2f max=%.2f\n", tempMin, tempMax);
    server.send(200, "text/plain", "OK");
}

// Blood pressure update
inline void handleSetBP() {
    if (server.hasArg("sys")) bpSys = server.arg("sys").toInt();
    if (server.hasArg("dia")) bpDia = server.arg("dia").toInt();

    Serial.printf("BP updated: %d/%d\n", bpSys, bpDia);
    server.send(200, "text/plain", "OK");
}

// Alert trigger (simulate button)
inline void handleAlert() {
    Serial.println("🚨 ALERT TRIGGERED FROM WEB");
    triggerAlert(TOPIC_ALERTS, alertType.emergencyButton);
    server.send(200, "text/plain", "Alert Sent");
}

// ================= INIT =================

inline void initWebServer() {
    server.on("/", handleRoot);
    server.on("/setTemp", handleSetTemp);
    server.on("/setBP", handleSetBP);
    server.on("/alert", handleAlert);

    server.begin();
}
