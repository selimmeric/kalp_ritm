#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <WiFi.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "body_svg.h" 

// Fonksiyon prototipleri
void handleRoot();
void handleStatus();
void handleSet();
void handleReset();
void handleNotFound();
void handleSave();
void handleLoad();
void handleViewSettings(); // YENİ: Dosya Görüntüleyici Fonksiyonu

// .ino dosyasındaki fonksiyonları burada bildirme
extern void saveSettings();
extern void loadSettings();

const char* ssid = "PulseSimAP";
const char* password = "12345678";
WebServer server(80);

void setupWebInterface() {
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP adresi: ");
  Serial.println(myIP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/save", HTTP_GET, handleSave);
  server.on("/load", HTTP_GET, handleLoad);
  server.on("/viewsettings", HTTP_GET, handleViewSettings); // YENİ: Dosya Görüntüleyici Adresi
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP sunucusu baslatildi.");
}

void handleWebRequests() {
  server.handleClient();
}

void handleSave() {
  saveSettings();
  server.send(200, "application/json", "{\"status\":\"saved\"}");
}

void handleLoad() {
  loadSettings();
  server.send(200, "application/json", "{\"status\":\"loaded\"}");
}

// YENİ FONKSİYON: settings.json dosyasının içeriğini web'de gösterir.
void handleViewSettings() {
  if (LittleFS.exists(CONFIG_FILE)) {
    File configFile = LittleFS.open(CONFIG_FILE, "r");
    server.streamFile(configFile, "application/json");
    configFile.close();
  } else {
    server.send(404, "text/plain", "Ayar dosyasi (settings.json) bulunamadi.");
  }
}

void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html lang="tr">
<head>
  <meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Anatomik Nabız Simülatörü</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; margin: 0; padding: 15px; background-color: #f8f9fa; color: #343a40; }
    h1 { font-size: 1.8rem; text-align: center; color: #2c3e50; margin-bottom: 20px; }
    .header-controls { display: flex; flex-wrap: wrap; gap: 10px; margin-bottom: 20px; }
    .header-controls button { flex: 1 1 auto; padding: 12px; font-size: 1rem; color: white; border: none; border-radius: 5px; cursor: pointer; transition: background-color 0.2s; }
    .save-btn { background-color: #28a745; } .save-btn:hover { background-color: #218838; }
    .load-btn { background-color: #007bff; } .load-btn:hover { background-color: #0069d9; }
    .reset-btn { background-color: #dc3545; } .reset-btn:hover { background-color: #c82333; }
    .debug-btn { background-color: #ffc107; color:#212529; } .debug-btn:hover { background-color: #e0a800; }
    .main-container { display: flex; flex-direction: column; max-width: 1600px; margin: auto; }
    #svg-container { flex: 1; min-width: 280px; padding: 10px; align-self: center; }
    #svg-container svg { width: 100%; height: auto; max-width: 400px; display: block; margin: auto; }
    .pulse-point { cursor: pointer; transition: all 0.2s; }
    .pulse-point:hover { opacity: 0.7; }
    .pulse-point.active { stroke: #007bff; stroke-width: 1.5px; transform-origin: center; transform: scale(1.2); }
    .actuator-cards-container { flex: 2; max-height: 90vh; overflow-y: auto; padding: 10px; }
    .actuator-container { display: grid; grid-template-columns: repeat(auto-fill, minmax(340px, 1fr)); gap: 15px; }
    .actuator-panel { background-color: white; border: 1px solid #dee2e6; padding: 15px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.05); transition: all 0.3s; }
    .actuator-panel.active { box-shadow: 0 0 15px rgba(0, 123, 255, 0.6); border-color: #007bff; }
    .actuator-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
    .actuator-header h2 { font-size: 1.1rem; margin: 0; color: #343a40; line-height: 1.3; }
    .control-group { display: flex; align-items: center; margin: 12px 0; font-size: .9rem; }
    .control-group label:first-child { width: 120px; margin-right: 10px; flex-shrink: 0; color: #495057; }
    .control-group span:last-of-type { margin-left: 8px; font-weight: 500; }
    input[type=range] { flex-grow: 1; margin: 0 5px; }
    .value-display { font-size: .9em; color: #007bff; width: 45px; text-align: right; flex-shrink: 0; font-weight: bold; }
    .switch { position: relative; display: inline-block; width: 50px; height: 28px; vertical-align: middle; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ced4da; border-radius: 28px; transition: .4s; }
    .slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 4px; bottom: 4px; background-color: white; border-radius: 50%; transition: .4s; box-shadow: 0 1px 3px rgba(0,0,0,0.2); }
    input:checked + .slider { background-color: #28a745; }
    input:checked + .slider:before { transform: translateX(22px); }
    .sub-control { display: none; padding-left: 15px; border-left: 3px solid #e9ecef; margin-top: 15px; }
    .sub-control.active { display: block; }
    .pqrst-label { font-weight: bold; color: #495057; margin-bottom: 5px; padding-top: 10px; border-top: 1px solid #e9ecef;}
    @media (min-width: 1024px) { .main-container { flex-direction: row; } .actuator-cards-container { border-left: 1px solid #dee2e6; } }
  </style>
</head>
<body>
  <h1>Anatomik Nabız Simülatörü</h1>
  <div class="header-controls">
    <button class="save-btn" onclick="saveAllSettings()">Ayarları Kaydet</button>
    <button class="load-btn" onclick="loadSavedSettings()">Kayıtlı Ayarları Yükle</button>
    <button class="reset-btn" onclick="resetAllValves()">Varsayılan Ayarlara Dön</button>
    <button class="debug-btn" onclick="viewSettings()">Kaydı Görüntüle</button>
  </div>
  <div class="main-container">
    <div id="svg-container">)=====";

  html += BODY_SVG;

  html += R"=====(</div>
    <div class="actuator-cards-container">
      <div class="actuator-container" id="actuators-container"></div>
    </div>
  </div>
  <script>
    const actuatorNames = [ "Temporal (Şakak)", "Carotid (Şah Damarı)", "Brachial (Kol) - Sol", "Brachial (Kol) - Sağ", "Radial (Bilek) - Sol", "Radial (Bilek) - Sağ", "Apical (Kalp)", "Femoral (Kasık) - Sol", "Femoral (Kasık) - Sağ", "Popliteal (Diz Arkası) - Sol", "Popliteal (Diz Arkası) - Sağ", "Posterior Tibial - Sol", "Posterior Tibial - Sağ", "Dorsalis Pedis - Sol", "Dorsalis Pedis - Sağ", "Subclavian (Köprücük)" ];
    
    document.addEventListener('DOMContentLoaded', function() {
      const container = document.getElementById('actuators-container');
      for (let i = 0; i < 16; i++) { container.insertAdjacentHTML('beforeend', createActuatorCard(i)); }
      addEventListeners(); addSvgListeners(); fetchData(); setInterval(fetchData, 2000);
    });

    function viewSettings() {
      window.open('/viewsettings', '_blank');
    }

    // Diğer tüm JS fonksiyonları (addSvgListeners, createActuatorCard vb.) öncekiyle tamamen aynıdır.
    function addSvgListeners() { const points = document.querySelectorAll('.pulse-point'); points.forEach(point => { point.addEventListener('click', () => { points.forEach(p => p.classList.remove('active')); document.querySelectorAll('.actuator-panel').forEach(c => c.classList.remove('active')); point.classList.add('active'); const index = point.id.split('_')[2]; const card = document.getElementById(`actuator-${index}`); if (card) { card.classList.add('active'); card.scrollIntoView({ behavior: 'smooth', block: 'center' }); } }); }); }
    function createActuatorCard(id) { return `<div class="actuator-panel" id="actuator-${id}"><div class="actuator-header"><h2>Aktüatör ${id + 1}<br><small style="color:#6c757d;">${actuatorNames[id]}</small></h2><label class="switch"><input type="checkbox" id="actuator-switch-${id}"><span class="slider"></span></label></div><div class="control-group"><label>Mod:</label><label class="switch"><input type="checkbox" id="mode-switch-${id}"><span class="slider"></span></label><span id="mode-text-${id}"></span></div><div class="control-group"><label for="pwm-${id}">Maksimum Güç:</label><input type="range" id="pwm-${id}" min="0" max="4095" oninput="this.nextElementSibling.textContent=this.value" onchange="updateActuator(${id},'pwmValue',parseInt(this.value))"><span class="value-display" id="pwm-value-${id}">0</span></div><div id="rhythm-controls-${id}" class="sub-control"><div class="control-group"><label>Ritim Tipi:</label><label class="switch"><input type="checkbox" id="rhythm-type-switch-${id}"><span class="slider"></span></label><span id="rhythm-type-text-${id}"></span></div><div class="control-group"><label for="bpm-${id}">BPM:</label><input type="range" id="bpm-${id}" min="40" max="140" oninput="this.nextElementSibling.textContent=this.value" onchange="updateActuator(${id},'heartRate',parseInt(this.value))"><span class="value-display" id="bpm-value-${id}">60</span></div><div id="simple-rhythm-settings-${id}" class="sub-control"><div class="control-group"><label>Atış Süresi Tipi:</label><label class="switch"><input type="checkbox" id="pulse-mode-switch-${id}"><span class="slider"></span></label><span id="pulse-mode-text-${id}"></span></div><div class="control-group"><label for="pulse-duration-${id}">Manuel Süre:</label><input type="range" id="pulse-duration-${id}" min="0" max="200" oninput="this.nextElementSibling.textContent=this.value+' ms'" onchange="updateActuator(${id},'manualPulseDuration',parseInt(this.value))"><span class="value-display" id="pulse-duration-value-${id}"></span></div></div><div id="pqrst-rhythm-settings-${id}" class="sub-control"><div class="pqrst-label">Zamanlama Ayarları (ms)</div><div class="control-group"><label>P Dalgası Süre:</label><input type="range" min="0" max="200" id="p-dur-${id}" oninput="this.nextElementSibling.textContent=this.value+' ms'" onchange="updateActuator(${id}, 'pWaveDuration', parseInt(this.value))"><span class="value-display" id="p-dur-val-${id}"></span></div><div class="control-group"><label>P-R Aralığı:</label><input type="range" min="0" max="200" id="pr-dur-${id}" oninput="this.nextElementSibling.textContent=this.value+' ms'" onchange="updateActuator(${id}, 'prSegmentDuration', parseInt(this.value))"><span class="value-display" id="pr-dur-val-${id}"></span></div><div class="control-group"><label>R Dalgası Süre:</label><input type="range" min="0" max="200" id="r-dur-${id}" oninput="this.nextElementSibling.textContent=this.value+' ms'" onchange="updateActuator(${id}, 'rWaveDuration', parseInt(this.value))"><span class="value-display" id="r-dur-val-${id}"></span></div><div class="control-group"><label>S-T Aralığı:</label><input type="range" min="0" max="200" id="st-dur-${id}" oninput="this.nextElementSibling.textContent=this.value+' ms'" onchange="updateActuator(${id}, 'stSegmentDuration', parseInt(this.value))"><span class="value-display" id="st-dur-val-${id}"></span></div><div class="control-group"><label>T Dalgası Süre:</label><input type="range" min="0" max="200" id="t-dur-${id}" oninput="this.nextElementSibling.textContent=this.value+' ms'" onchange="updateActuator(${id}, 'tWaveDuration', parseInt(this.value))"><span class="value-display" id="t-dur-val-${id}"></span></div><div class="pqrst-label">Güç Ayarları (% Maks.)</div><div class="control-group"><label>P Dalgası Güç:</label><input type="range" min="0" max="100" id="p-pwm-${id}" oninput="this.nextElementSibling.textContent=this.value+'%'" onchange="updateActuator(${id}, 'pWavePwm', parseInt(this.value))"><span class="value-display" id="p-pwm-val-${id}"></span></div><div class="control-group"><label>R Dalgası Güç:</label><input type="range" min="0" max="100" id="r-pwm-${id}" oninput="this.nextElementSibling.textContent=this.value+'%'" onchange="updateActuator(${id}, 'rWavePwm', parseInt(this.value))"><span class="value-display" id="r-pwm-val-${id}"></span></div><div class="control-group"><label>T Dalgası Güç:</label><input type="range" min="0" max="100" id="t-pwm-${id}" oninput="this.nextElementSibling.textContent=this.value+'%'" onchange="updateActuator(${id}, 'tWavePwm', parseInt(this.value))"><span class="value-display" id="t-pwm-val-${id}"></span></div></div></div></div>`;}
    function addEventListeners() { for (let i = 0; i < 16; i++) { document.getElementById(`actuator-switch-${i}`).addEventListener('change', e => updateActuator(i, 'state', e.target.checked)); document.getElementById(`mode-switch-${i}`).addEventListener('change', e => { const isRhythmMode = e.target.checked; updateActuator(i, 'mode', isRhythmMode ? 'HEART_RHYTHM' : 'MANUAL_PWM'); document.getElementById(`rhythm-controls-${id}`).classList.toggle('active', isRhythmMode); }); document.getElementById(`rhythm-type-switch-${i}`).addEventListener('change', e => { const isPQRST = e.target.checked; updateActuator(i, 'usePQRST', isPQRST); document.getElementById(`simple-rhythm-settings-${i}`).classList.toggle('active', isRhythmMode && !isPQRST); document.getElementById(`pqrst-rhythm-settings-${i}`).classList.toggle('active', isRhythmMode && isPQRST); }); document.getElementById(`pulse-mode-switch-${i}`).addEventListener('change', e => updateActuator(i, 'useDynamicPulse', e.target.checked)); } }
    function updateActuator(id, key, value) { const payload = { id, [key]: value }; fetch('/set', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) }).catch(error => console.error('Hata:', error)); }
    function updateUI(id, data) { document.getElementById(`actuator-switch-${id}`).checked = data.state; const isRhythmMode = data.mode === 'HEART_RHYTHM'; document.getElementById(`mode-switch-${id}`).checked = isRhythmMode; document.getElementById(`mode-text-${id}`).textContent = isRhythmMode ? 'Ritim Modu' : 'Manuel Kontrol'; document.getElementById(`rhythm-controls-${id}`).classList.toggle('active', isRhythmMode); document.getElementById(`pwm-${id}`).value = data.pwmValue; document.getElementById(`pwm-value-${id}`).textContent = data.pwmValue; document.getElementById(`bpm-${id}`).value = data.heartRate; document.getElementById(`bpm-value-${id}`).textContent = data.heartRate; const isPQRST = data.usePQRST; document.getElementById(`rhythm-type-switch-${id}`).checked = isPQRST; document.getElementById(`rhythm-type-text-${id}`).textContent = isPQRST ? 'PQRST Kompleks' : 'Basit Atış'; document.getElementById(`simple-rhythm-settings-${id}`).classList.toggle('active', isRhythmMode && !isPQRST); document.getElementById(`pqrst-rhythm-settings-${id}`).classList.toggle('active', isRhythmMode && isPQRST); document.getElementById(`pulse-mode-switch-${id}`).checked = data.useDynamicPulse; document.getElementById(`pulse-mode-text-${id}`).textContent = data.useDynamicPulse ? 'Dinamik' : 'Manuel'; document.getElementById(`pulse-duration-${id}`).value = data.manualPulseDuration; document.getElementById(`pulse-duration-value-${id}`).textContent = data.manualPulseDuration + ' ms'; document.getElementById(`p-dur-${id}`).value = data.pWaveDuration; document.getElementById(`p-dur-val-${id}`).textContent = data.pWaveDuration + ' ms'; document.getElementById(`pr-dur-${id}`).value = data.prSegmentDuration; document.getElementById(`pr-dur-val-${id}`).textContent = data.prSegmentDuration + ' ms'; document.getElementById(`r-dur-${id}`).value = data.rWaveDuration; document.getElementById(`r-dur-val-${id}`).textContent = data.rWaveDuration + ' ms'; document.getElementById(`st-dur-${id}`).value = data.stSegmentDuration; document.getElementById(`st-dur-val-${id}`).textContent = data.stSegmentDuration + ' ms'; document.getElementById(`t-dur-${id}`).value = data.tWaveDuration; document.getElementById(`t-dur-val-${id}`).textContent = data.tWaveDuration + ' ms'; document.getElementById(`p-pwm-${id}`).value = data.pWavePwm; document.getElementById(`p-pwm-val-${id}`).textContent = data.pWavePwm + '%'; document.getElementById(`r-pwm-${id}`).value = data.rWavePwm; document.getElementById(`r-pwm-val-${id}`).textContent = data.rWavePwm + '%'; document.getElementById(`t-pwm-${id}`).value = data.tWavePwm; document.getElementById(`t-pwm-val-${id}`).textContent = data.tWavePwm + '%';}
    async function fetchData() { try { const response = await fetch('/status'); if (!response.ok) throw new Error('Sunucu durumu alinamadi'); const data = await response.json(); data.valves.forEach((actuatorData, id) => updateUI(id, actuatorData)); } catch (error) { console.error('Veri alim hatasi:', error); } }
    function saveAllSettings() { if (!confirm('Mevcut ayarlar tüm aktüatörler için kalıcı olarak kaydedilsin mi?')) return; fetch('/save').then(res => res.ok ? alert('Ayarlar başarıyla kaydedildi!') : alert('Kaydetme başarısız!')); }
    function loadSavedSettings() { if (!confirm('Kaydedilmiş ayarlar yüklensin mi? Mevcut değişiklikler kaybolacak.')) return; fetch('/load').then(() => fetchData()); }
    function resetAllValves() { if (!confirm('Tüm ayarlar varsayılanlara döndürülsün mü?')) return; fetch('/reset').then(() => fetchData()); }
  </script>
</body>
</html>
)=====";
  server.send(200, "text/html", html);
}

// C++ handler fonksiyonlarının geri kalanı (handleSet, handleStatus vb.) öncekiyle aynıdır ve değişmesine gerek yoktur.
void handleSet() { if (server.method() != HTTP_POST) return; JsonDocument doc; if (deserializeJson(doc, server.arg("plain"))) return; int id = doc["id"]; if (id < 0 || id >= NUM_VALVES) return; if (doc.containsKey("state")) valves[id].state = doc["state"]; if (doc.containsKey("mode")) valves[id].mode = (strcmp(doc["mode"], "MANUAL_PWM") == 0) ? MANUAL_PWM : HEART_RHYTHM; if (doc.containsKey("pwmValue")) valves[id].pwmValue = doc["pwmValue"]; if (doc.containsKey("heartRate")) { valves[id].heartRate = doc["heartRate"]; valves[id].beatInterval = 60000 / valves[id].heartRate; } if (doc.containsKey("useDynamicPulse")) valves[id].useDynamicPulse = doc["useDynamicPulse"]; if (doc.containsKey("manualPulseDuration")) valves[id].manualPulseDuration = doc["manualPulseDuration"]; if (doc.containsKey("usePQRST")) valves[id].usePQRST = doc["usePQRST"]; if (doc.containsKey("pWaveDuration")) valves[id].pWaveDuration = doc["pWaveDuration"]; if (doc.containsKey("prSegmentDuration")) valves[id].prSegmentDuration = doc["prSegmentDuration"]; if (doc.containsKey("rWaveDuration")) valves[id].rWaveDuration = doc["rWaveDuration"]; if (doc.containsKey("stSegmentDuration")) valves[id].stSegmentDuration = doc["stSegmentDuration"]; if (doc.containsKey("tWaveDuration")) valves[id].tWaveDuration = doc["tWaveDuration"]; if (doc.containsKey("pWavePwm")) valves[id].pWavePwm = doc["pWavePwm"]; if (doc.containsKey("rWavePwm")) valves[id].rWavePwm = doc["rWavePwm"]; if (doc.containsKey("tWavePwm")) valves[id].tWavePwm = doc["tWavePwm"]; if (valves[id].state && valves[id].mode == MANUAL_PWM) { setSolenoidDuty(id, valves[id].pwmValue); } else if (!valves[id].state) { setSolenoidDuty(id, 0); } server.send(200, "application/json", "{\"status\":\"ok\"}");}
void handleStatus() { JsonDocument doc; JsonArray valvesArray = doc["valves"].to<JsonArray>(); for (int i = 0; i < NUM_VALVES; i++) { JsonObject v = valvesArray.add<JsonObject>(); v["state"] = valves[i].state; v["mode"] = (valves[i].mode == MANUAL_PWM) ? "MANUAL_PWM" : "HEART_RHYTHM"; v["pwmValue"] = valves[i].pwmValue; v["heartRate"] = valves[i].heartRate; v["useDynamicPulse"] = valves[i].useDynamicPulse; v["manualPulseDuration"] = valves[i].manualPulseDuration; v["usePQRST"] = valves[i].usePQRST; v["pWaveDuration"] = valves[i].pWaveDuration; v["prSegmentDuration"] = valves[i].prSegmentDuration; v["rWaveDuration"] = valves[i].rWaveDuration; v["stSegmentDuration"] = valves[i].stSegmentDuration; v["tWaveDuration"] = valves[i].tWaveDuration; v["pWavePwm"] = valves[i].pWavePwm; v["rWavePwm"] = valves[i].rWavePwm; v["tWavePwm"] = valves[i].tWavePwm; } String response; serializeJson(doc, response); server.send(200, "application/json", response);}
void handleReset() { for (int i = 0; i < NUM_VALVES; i++) { valves[i] = { .channel = (uint8_t)i, .mode = MANUAL_PWM, .state = false, .pwmValue = 0, .heartRate = 60, .beatInterval = 1000, .lastBeatTime = 0, .lastDutyCycle = -1, .debug = false, .useDynamicPulse = true, .manualPulseDuration = 50, .usePQRST = false, .pqrstState = STATE_IDLE, .lastPqrstEventTime = 0, .pWaveDuration = 80, .prSegmentDuration = 70, .rWaveDuration = 100, .stSegmentDuration = 100, .tWaveDuration = 160, .pWavePwm = 40, .rWavePwm = 100, .tWavePwm = 60 }; setSolenoidDuty(i, 0); } saveSettings(); server.send(200, "application/json", "{\"status\":\"reset_done_and_saved\"}");}
void handleNotFound() { server.send(404, "text/plain", "404: Not Found");}

#endif