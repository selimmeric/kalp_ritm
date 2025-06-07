#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <LittleFS.h> // DOSYA SİSTEMİ İÇİN EKLENDİ

#define PWM_FREQUENCY 1000
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

#define MIN_BPM 40
#define MAX_BPM 140
#define NUM_VALVES 16
#define CONFIG_FILE "/settings.json" // Ayar dosyamızın adı

enum ControlMode { MANUAL_PWM, HEART_RHYTHM };
enum PQRST_State { STATE_IDLE, STATE_P_WAVE, STATE_PR_SEGMENT, STATE_R_WAVE, STATE_ST_SEGMENT, STATE_T_WAVE };

struct ValveControl {
  uint8_t channel; ControlMode mode; bool state; int pwmValue; int heartRate; unsigned long beatInterval;
  unsigned long lastBeatTime; int lastDutyCycle; bool debug; bool useDynamicPulse; unsigned int manualPulseDuration;
  bool usePQRST; PQRST_State pqrstState; unsigned long lastPqrstEventTime; unsigned int pWaveDuration, prSegmentDuration, rWaveDuration, stSegmentDuration, tWaveDuration;
  unsigned int pWavePwm, rWavePwm, tWavePwm;
};

ValveControl valves[NUM_VALVES];
void setSolenoidDuty(uint8_t channel, int dutyCycle);

// Prototip bildirimleri
void saveSettings();
void loadSettings();

#include "webinterface.h"

void setup() {
  Serial.begin(115200);
  Serial.println("Ayarlanabilir PQRST Kontrol Sistemi - Kalici Hafiza Aktif");
  
  // LittleFS'i başlat
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS baslatilamadi!");
    return;
  }
  Serial.println("LittleFS baslatildi.");

  pwm.begin();
  pwm.setPWMFreq(PWM_FREQUENCY);

  // Ayarları dosyadan yükle, eğer dosya yoksa varsayılanları kullan ve dosyayı oluştur.
  loadSettings(); 

  setupWebInterface();
}

void loop() {
  handleWebRequests();
  for (int i = 0; i < NUM_VALVES; i++) {
    handleHeartRhythm(i);
  }
  delay(1);
}

void saveSettings() {
  StaticJsonDocument<8192> doc; // Dinamik yerine büyük bir Statik JSON belgesi kullanıyoruz (8KB)
  JsonArray valvesArray = doc["valves"].to<JsonArray>();
  for (int i = 0; i < NUM_VALVES; i++) {
    JsonObject v = valvesArray.add<JsonObject>();
    // ... (tüm atamalar aynı)
    v["state"] = valves[i].state; v["mode"] = valves[i].mode; v["pwmValue"] = valves[i].pwmValue;
    v["heartRate"] = valves[i].heartRate; v["useDynamicPulse"] = valves[i].useDynamicPulse;
    v["manualPulseDuration"] = valves[i].manualPulseDuration; v["usePQRST"] = valves[i].usePQRST;
    v["pWaveDuration"] = valves[i].pWaveDuration; v["prSegmentDuration"] = valves[i].prSegmentDuration;
    v["rWaveDuration"] = valves[i].rWaveDuration; v["stSegmentDuration"] = valves[i].stSegmentDuration;
    v["tWaveDuration"] = valves[i].tWaveDuration; v["pWavePwm"] = valves[i].pWavePwm;
    v["rWavePwm"] = valves[i].rWavePwm; v["tWavePwm"] = valves[i].tWavePwm;
  }

  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("Ayar dosyasi kaydedilemedi!");
    return;
  }
  if (serializeJson(doc, configFile) == 0) {
    Serial.println("JSON dosyaya yazilamadi.");
  } else {
    Serial.println("Ayarlar basariyla kaydedildi.");
  }
  configFile.close();
}

void loadSettings() {
  if (LittleFS.exists(CONFIG_FILE)) {
    File configFile = LittleFS.open(CONFIG_FILE, "r");
    if (!configFile) {
      Serial.println("Ayar dosyasi acilamadi!");
      return;
    }
    StaticJsonDocument<8192> doc; // Okurken de büyük Statik belge kullan
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
    if (error) {
      Serial.print("JSON parse hatasi: ");
      Serial.println(error.c_str());
      return;
    }

    JsonArray valvesArray = doc["valves"];
    for (int i = 0; i < NUM_VALVES; i++) {
      JsonObject v = valvesArray[i];
      valves[i].state = v["state"]; valves[i].mode = v["mode"]; valves[i].pwmValue = v["pwmValue"];
      valves[i].heartRate = v["heartRate"]; valves[i].useDynamicPulse = v["useDynamicPulse"];
      valves[i].manualPulseDuration = v["manualPulseDuration"]; valves[i].usePQRST = v["usePQRST"];
      valves[i].pWaveDuration = v["pWaveDuration"]; valves[i].prSegmentDuration = v["prSegmentDuration"];
      valves[i].rWaveDuration = v["rWaveDuration"]; valves[i].stSegmentDuration = v["stSegmentDuration"];
      valves[i].tWaveDuration = v["tWaveDuration"]; valves[i].pWavePwm = v["pWavePwm"];
      valves[i].rWavePwm = v["rWavePwm"]; valves[i].tWavePwm = v["tWavePwm"];
      valves[i].beatInterval = 60000 / valves[i].heartRate;
      valves[i].channel = i;
    }
    Serial.println("Ayarlar dosyadan basariyla yuklendi.");
  } else {
    Serial.println("Ayar dosyasi bulunamadi. Varsayilan ayarlar kullaniliyor ve yeni dosya olusturuluyor.");
    // Varsayılan değerleri ata (setup içindeki gibi)
    for (int i = 0; i < NUM_VALVES; i++) {
        valves[i] = {
          .channel = (uint8_t)i, .mode = MANUAL_PWM, .state = false,
          .pwmValue = 0, .heartRate = 60, .beatInterval = 1000, .lastBeatTime = 0, .lastDutyCycle = -1, .debug = false,
          .useDynamicPulse = true, .manualPulseDuration = 50, .usePQRST = false, .pqrstState = STATE_IDLE, .lastPqrstEventTime = 0,
          .pWaveDuration = 80, .prSegmentDuration = 70, .rWaveDuration = 100, .stSegmentDuration = 100, .tWaveDuration = 160,
          .pWavePwm = 40, .rWavePwm = 100, .tWavePwm = 60
        };
    }
    saveSettings(); // Varsayılanları dosyaya kaydet
  }
}

// handleHeartRhythm ve diğer fonksiyonlar öncekiyle aynı...
void setSolenoidDuty(uint8_t channel, int dutyCycle) {
  dutyCycle = constrain(dutyCycle, 0, 4095);
  if (dutyCycle != valves[channel].lastDutyCycle) {
    pwm.setPWM(channel, 0, dutyCycle);
    valves[channel].lastDutyCycle = dutyCycle;
  }
}
void handleHeartRhythm(int valveIndex) {
  unsigned long currentTime = millis();
  ValveControl &valve = valves[valveIndex];
  if (!valve.state || valve.mode != HEART_RHYTHM) {
    if (valve.pqrstState != STATE_IDLE) {
        setSolenoidDuty(valve.channel, 0);
        valve.pqrstState = STATE_IDLE;
    }
    return;
  }
  if (valve.usePQRST) {
    if (valve.pqrstState == STATE_IDLE && (currentTime - valve.lastBeatTime >= valve.beatInterval)) {
      valve.lastBeatTime = currentTime; valve.pqrstState = STATE_P_WAVE; valve.lastPqrstEventTime = currentTime;
      int p_power = (valve.pwmValue * valve.pWavePwm) / 100;
      setSolenoidDuty(valve.channel, p_power); 
    }
    switch (valve.pqrstState) {
      case STATE_P_WAVE:
        if (currentTime - valve.lastPqrstEventTime > valve.pWaveDuration) {
          setSolenoidDuty(valve.channel, 0); valve.pqrstState = STATE_PR_SEGMENT; valve.lastPqrstEventTime = currentTime;
        }
        break;
      case STATE_PR_SEGMENT:
        if (currentTime - valve.lastPqrstEventTime > valve.prSegmentDuration) {
          int r_power = (valve.pwmValue * valve.rWavePwm) / 100;
          setSolenoidDuty(valve.channel, r_power); valve.pqrstState = STATE_R_WAVE; valve.lastPqrstEventTime = currentTime;
        }
        break;
      case STATE_R_WAVE:
        if (currentTime - valve.lastPqrstEventTime > valve.rWaveDuration) {
          setSolenoidDuty(valve.channel, 0); valve.pqrstState = STATE_ST_SEGMENT; valve.lastPqrstEventTime = currentTime;
        }
        break;
      case STATE_ST_SEGMENT:
        if (currentTime - valve.lastPqrstEventTime > valve.stSegmentDuration) {
          int t_power = (valve.pwmValue * valve.tWavePwm) / 100;
          setSolenoidDuty(valve.channel, t_power); valve.pqrstState = STATE_T_WAVE; valve.lastPqrstEventTime = currentTime;
        }
        break;
      case STATE_T_WAVE:
        if (currentTime - valve.lastPqrstEventTime > valve.tWaveDuration) {
          setSolenoidDuty(valve.channel, 0); valve.pqrstState = STATE_IDLE;
        }
        break;
      case STATE_IDLE: break;
    }
  } else {
    static unsigned long pulseStartTime[NUM_VALVES] = {0}; static bool isPulsing[NUM_VALVES] = {false};
    unsigned long currentPulseDuration = valve.useDynamicPulse ? max(20UL, valve.beatInterval / 4) : valve.manualPulseDuration;
    if (!isPulsing[valveIndex] && (currentTime - valve.lastBeatTime >= valve.beatInterval)) {
      setSolenoidDuty(valve.channel, valve.pwmValue); valve.lastBeatTime = currentTime; pulseStartTime[valveIndex] = currentTime; isPulsing[valveIndex] = true;
    }
    if (isPulsing[valveIndex] && (currentTime - pulseStartTime[valveIndex] >= currentPulseDuration)) {
      setSolenoidDuty(valve.channel, 0); isPulsing[valveIndex] = false;
    }
  }
}