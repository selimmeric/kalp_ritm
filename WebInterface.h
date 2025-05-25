#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <WiFi.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <ArduinoJson.h>

void handleRoot();
void handleWebRequests();
void handleControl();
void handleStatus();
void handleSet();
// WiFi credentials
const char* ssid = "ValveControllerAP";
const char* password = "12345678";

WebServer server(80);

void setupWebInterface() {
  // Start WiFi AP
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Serve static files
  server.on("/", HTTP_GET, handleRoot);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/set", HTTP_POST, handleSet);
  
  server.begin();
  Serial.println("HTTP server started");
}

void handleWebRequests() {
  server.handleClient();
}

void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>16 Valf Kontrol Sistemi</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; }
    .valve-container { display: grid; grid-template-columns: repeat(4, 1fr); gap: 15px; }
    .valve-card { border: 1px solid #ddd; padding: 15px; border-radius: 5px; }
    .valve-header { display: flex; justify-content: space-between; align-items: center; }
    .switch { position: relative; display: inline-block; width: 60px; height: 34px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }
    .slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: #2196F3; }
    input:checked + .slider:before { transform: translateX(26px); }
    .control-group { margin: 10px 0; }
    label { display: block; margin-bottom: 5px; }
    input[type="range"] { width: 100%; }
    .value-display { font-size: 0.9em; color: #666; }
  </style>
</head>
<body>
  <h1>16 Valf Kontrol Sistemi</h1>
  <div class="valve-container" id="valves-container">
    <!-- Valve cards will be inserted here by JavaScript -->
  </div>
  
  <script>
    // Global valve state object
    let valveStates = Array(16).fill().map(() => ({
      mode: 'MANUAL_PWM',
      pwmValue: 0,
      heartRate: 60,
      manualPulseDuration: 50,
      useDynamicPulse: true,
      state: false
    }));

    // DOM elements cache
    const valveElements = {};

    document.addEventListener('DOMContentLoaded', function() {
      // Initialize valve cards and cache elements
      const container = document.getElementById('valves-container');
      for (let i = 0; i < 16; i++) {
        const valveCard = createValveCard(i);
        container.innerHTML += valveCard;
        
        // Cache important elements for each valve
        valveElements[i] = {
          checkbox: document.querySelector(`#valve-switch-${i}`),
          pwmValue: document.querySelector(`#pwm-value-${i}`),
          bpmValue: document.querySelector(`#bpm-value-${i}`),
          pulseDuration: document.querySelector(`#pulse-duration-value-${i}`),
          modeSelect: document.querySelector(`#mode-${i}`),
          pulseModeSelect: document.querySelector(`#pulse-mode-${i}`),
          pwmSlider: document.querySelector(`#pwm-${i}`),
          bpmSlider: document.querySelector(`#bpm-${i}`),
          pulseSlider: document.querySelector(`#pulse-duration-${i}`)
        };
      }

      // Initial data fetch
      fetchData();
      
      // Periodic data fetch (every 2 seconds)
      setInterval(fetchData, 2000);
    });

    function createValveCard(id) {
      return `
        <div class="valve-card" id="valve-${id}">
          <div class="valve-header">
            <h2>Valf ${id + 1}</h2>
            <label class="switch">
              <input type="checkbox" id="valve-switch-${id}" onchange="toggleValve(${id}, this.checked)">
              <span class="slider"></span>
            </label>
          </div>
          
          <div class="control-group">
            <label for="mode-${id}">Mod:</label>
            <select id="mode-${id}" onchange="setMode(${id}, this.value)">
              <option value="MANUAL_PWM">Manuel PWM</option>
              <option value="HEART_RHYTHM">Kalp Ritmi</option>
            </select>
          </div>
          
          <div class="control-group">
            <label for="pwm-${id}">PWM Değeri (0-4095):</label>
            <input type="range" id="pwm-${id}" min="0" max="4095" value="0" 
              oninput="updatePwmValue(${id}, this.value)">
            <span class="value-display" id="pwm-value-${id}">0</span>
          </div>
          
          <div class="control-group">
            <label for="bpm-${id}">BPM (40-140):</label>
            <input type="range" id="bpm-${id}" min="40" max="140" value="60" 
              oninput="updateBpmValue(${id}, this.value)">
            <span class="value-display" id="bpm-value-${id}">60</span>
          </div>
          
          <div class="control-group">
            <label for="pulse-duration-${id}">Atış Süresi (ms):</label>
            <input type="range" id="pulse-duration-${id}" min="20" max="500" value="50" 
              oninput="updatePulseDuration(${id}, this.value)">
            <span class="value-display" id="pulse-duration-value-${id}">50</span>
          </div>
          
          <div class="control-group">
            <label for="pulse-mode-${id}">Atış Modu:</label>
            <select id="pulse-mode-${id}" onchange="setPulseMode(${id}, this.value)">
              <option value="DYNAMIC">Dinamik</option>
              <option value="MANUAL">Manuel</option>
            </select>
          </div>
        </div>
      `;
    }

    async function fetchData() {
      try {
        const response = await fetch('/status');
        if (!response.ok) throw new Error('Network response was not ok');
        
        const data = await response.json();
        
        data.valves.forEach(valve => {
          const id = valve.id;
          const currentState = valveStates[id];
          const elements = valveElements[id];
          
          // Update state only if changed
          if (JSON.stringify(currentState) !== JSON.stringify(valve)) {
            valveStates[id] = valve;
            
            // Update UI elements if they exist
            if (elements) {
              // Update checkbox if state changed
              const shouldBeChecked = valve.mode === 'MANUAL_PWM' && valve.pwmValue > 0;
              if (elements.checkbox.checked !== shouldBeChecked) {
                elements.checkbox.checked = shouldBeChecked;
              }
              
              // Update display values if changed
              if (elements.pwmValue.textContent != valve.pwmValue) {
                elements.pwmValue.textContent = valve.pwmValue;
                elements.pwmSlider.value = valve.pwmValue;
              }
              
              if (elements.bpmValue.textContent != valve.heartRate) {
                elements.bpmValue.textContent = valve.heartRate;
                elements.bpmSlider.value = valve.heartRate;
              }
              
              if (elements.pulseDuration.textContent != valve.manualPulseDuration) {
                elements.pulseDuration.textContent = valve.manualPulseDuration;
                elements.pulseSlider.value = valve.manualPulseDuration;
              }
              
              // Update select elements if changed
              if (elements.modeSelect.value !== valve.mode) {
                elements.modeSelect.value = valve.mode;
              }
              
              const pulseModeValue = valve.useDynamicPulse ? 'DYNAMIC' : 'MANUAL';
              if (elements.pulseModeSelect.value !== pulseModeValue) {
                elements.pulseModeSelect.value = pulseModeValue;
              }
            }
          }
        });
      } catch (error) {
        console.error('Error fetching data:', error);
        // Retry after a shorter interval if there's an error
        setTimeout(fetchData, 500);
      }
    }

    async function toggleValve(id, state) {
      try {
        const response = await fetch('/control', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id, state })
        });
        if (!response.ok) throw new Error('Control command failed');
      } catch (error) {
        console.error('Error toggling valve:', error);
        // Revert UI change if failed
        valveElements[id].checkbox.checked = !state;
      }
    }

    async function setMode(id, mode) {
      try {
        const response = await fetch('/set', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id, mode })
        });
        if (!response.ok) throw new Error('Mode change failed');
      } catch (error) {
        console.error('Error setting mode:', error);
        // Revert UI change if failed
        valveElements[id].modeSelect.value = valveStates[id].mode;
      }
    }

    async function updatePwmValue(id, value) {
      valveElements[id].pwmValue.textContent = value;
      try {
        const response = await fetch('/set', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id, pwmValue: parseInt(value) })
        });
        if (!response.ok) throw new Error('PWM update failed');
      } catch (error) {
        console.error('Error updating PWM:', error);
        // Revert UI change if failed
        valveElements[id].pwmValue.textContent = valveStates[id].pwmValue;
        valveElements[id].pwmSlider.value = valveStates[id].pwmValue;
      }
    }

    async function updateBpmValue(id, value) {
      valveElements[id].bpmValue.textContent = value;
      try {
        const response = await fetch('/set', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id, heartRate: parseInt(value) })
        });
        if (!response.ok) throw new Error('BPM update failed');
      } catch (error) {
        console.error('Error updating BPM:', error);
        // Revert UI change if failed
        valveElements[id].bpmValue.textContent = valveStates[id].heartRate;
        valveElements[id].bpmSlider.value = valveStates[id].heartRate;
      }
    }

    async function updatePulseDuration(id, value) {
      valveElements[id].pulseDuration.textContent = value;
      try {
        const response = await fetch('/set', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id, manualPulseDuration: parseInt(value) })
        });
        if (!response.ok) throw new Error('Pulse duration update failed');
      } catch (error) {
        console.error('Error updating pulse duration:', error);
        // Revert UI change if failed
        valveElements[id].pulseDuration.textContent = valveStates[id].manualPulseDuration;
        valveElements[id].pulseSlider.value = valveStates[id].manualPulseDuration;
      }
    }

    async function setPulseMode(id, mode) {
      try {
        const response = await fetch('/set', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ id, useDynamicPulse: mode === 'DYNAMIC' })
        });
        if (!response.ok) throw new Error('Pulse mode change failed');
      } catch (error) {
        console.error('Error setting pulse mode:', error);
        // Revert UI change if failed
        const currentMode = valveStates[id].useDynamicPulse ? 'DYNAMIC' : 'MANUAL';
        valveElements[id].pulseModeSelect.value = currentMode;
      }
    }
  </script>
</body>
</html>
)=====";

  server.send(200, "text/html", html);
}

void handleControl() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String body = server.arg("plain");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  int id = doc["id"];
  if (id < 0 || id >= NUM_VALVES) {
    server.send(400, "application/json", "{\"error\":\"Invalid valve ID\"}");
    return;
  }

  // containsKey yerine doc["key"].is<T>() kullanımı
  if (doc["state"].is<bool>()) {
    bool state = doc["state"];
    if (state) {
      setSolenoidDuty(id, valves[id].pwmValue);
    } else {
      setSolenoidDuty(id, 0);
    }
  }

  if (doc["bpm"].is<int>()) {
    int bpm = doc["bpm"];
    if (bpm >= MIN_BPM && bpm <= MAX_BPM) {
      valves[id].heartRate = bpm;
      valves[id].beatInterval = 60000 / bpm;
      valves[id].mode = HEART_RHYTHM;
    }
  }

  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleStatus() {
  JsonDocument doc;
  JsonArray valvesArray = doc["valves"].to<JsonArray>();

  for (int i = 0; i < NUM_VALVES; i++) {
    JsonObject valveObj = valvesArray.add<JsonObject>();
    valveObj["id"] = i;
    valveObj["mode"] = valves[i].mode == MANUAL_PWM ? "MANUAL_PWM" : "HEART_RHYTHM";
    valveObj["pwmValue"] = valves[i].pwmValue;
    valveObj["heartRate"] = valves[i].heartRate;
    valveObj["manualPulseDuration"] = valves[i].manualPulseDuration;
    valveObj["useDynamicPulse"] = valves[i].useDynamicPulse;
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSet() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String body = server.arg("plain");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  int id = doc["id"];
  if (id < 0 || id >= NUM_VALVES) {
    server.send(400, "application/json", "{\"error\":\"Invalid valve ID\"}");
    return;
  }

  // containsKey yerine doc["key"].is<T>() kullanımı
  if (doc["mode"].is<const char*>()) {
    const char* mode = doc["mode"];
    valves[id].mode = (strcmp(mode, "MANUAL_PWM") == 0) ? MANUAL_PWM : HEART_RHYTHM;
  }

  if (doc["pwmValue"].is<int>()) {
    valves[id].pwmValue = doc["pwmValue"];
    if (valves[id].mode == MANUAL_PWM) {
      setSolenoidDuty(id, valves[id].pwmValue);
    }
  }

  if (doc["heartRate"].is<int>()) {
    valves[id].heartRate = doc["heartRate"];
    valves[id].beatInterval = 60000 / valves[id].heartRate;
  }

  if (doc["manualPulseDuration"].is<int>()) {
    valves[id].manualPulseDuration = doc["manualPulseDuration"];
  }

  if (doc["useDynamicPulse"].is<bool>()) {
    valves[id].useDynamicPulse = doc["useDynamicPulse"];
  }

  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

#endif