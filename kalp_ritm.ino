#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <tinyCommand.hpp>

// PCA9685 ayarları
#define PWM_FREQUENCY 1000 // Hz cinsinden PWM frekansı
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Sabitler
#define MIN_BPM 40
#define MAX_BPM 140
#define NUM_VALVES 16 // PCA9685 ile 16 kanal destekleniyor
#define DEFAULT_PULSE_LENGTH 50 // Varsayılan atış süresi (ms)

// Durumlar
enum ControlMode {
  MANUAL_PWM,
  HEART_RHYTHM
};

// Valf Kontrol Yapısı
struct ValveControl {
  uint8_t channel; // PCA9685 kanal numarası (0-15)
  ControlMode mode;
  unsigned long lastBeatTime;
  unsigned long beatInterval;
  int heartRate;
  int pwmValue;
  unsigned long manualPulseDuration;
  bool useDynamicPulse;
  int lastDutyCycle;
  bool debug;
};

// Valf Dizisi
ValveControl valves[NUM_VALVES];

// tinyCommand nesnesi
static tinyCommand cmd(Serial);

void setSolenoidDuty(uint8_t channel, int dutyCycle);

#include "WebInterface.h"
void setup() {
  Serial.begin(115200);
  Serial.println("PCA9685 ile 16 Valf Kontrol Sistemi");
  
  // PCA9685 başlatma
  pwm.begin();
  pwm.setPWMFreq(PWM_FREQUENCY);
  
  cmd.begin();
  cmd.setCmd("valf", set_valf_komutu);
  cmd.setCmd("pwm", set_pwm);
  cmd.setCmd("bpm", set_bpm);
  cmd.setCmd("mode", set_mode);
  cmd.setCmd("pdur", set_pulse_duration);
  cmd.setCmd("pdmode", set_pulse_mode);
  cmd.setCmd("debug", set_debug);

  // Valf başlangıç ayarları
  for (int i = 0; i < NUM_VALVES; i++) {
    valves[i].channel = i;
    valves[i].mode = MANUAL_PWM;
    valves[i].lastBeatTime = 0;
    valves[i].beatInterval = 60000 / 60; // 60 BPM varsayılan
    valves[i].heartRate = 60;
    valves[i].pwmValue = 0;
    valves[i].manualPulseDuration = DEFAULT_PULSE_LENGTH;
    valves[i].useDynamicPulse = true;
    valves[i].lastDutyCycle = -1;
    valves[i].debug = false;
    
    // Başlangıçta tüm valfler kapalı
    setSolenoidDuty(i, 0);
  }

  setupWebInterface();
}

void loop() {
  cmd.scan();
  handleWebRequests();
  for (int i = 0; i < NUM_VALVES; i++) {
    handleHeartRhythm(i);
  }
  delay(1);
}

void setSolenoidDuty(uint8_t channel, int dutyCycle) {
  if (dutyCycle < 0) dutyCycle = 0;
  if (dutyCycle > 4095) dutyCycle = 4095; // PCA9685 12-bit çözünürlük
  
  pwm.setPWM(channel, 0, dutyCycle);
  
  if (dutyCycle != valves[channel].lastDutyCycle) {
    if (valves[channel].debug) {
      Serial.print("Valf ");
      Serial.print(channel + 1);
      Serial.print(" (Kanal ");
      Serial.print(channel);
      Serial.print(") duty cycle: ");
      Serial.println(dutyCycle);
    }
    valves[channel].lastDutyCycle = dutyCycle;
  }
}

void handleHeartRhythm(int valveIndex) {
  static unsigned long pulseStartTime[NUM_VALVES] = {0};
  static bool isPulsing[NUM_VALVES] = {false};
  
  unsigned long currentTime = millis();
  unsigned long minPulseDuration = 20; // Minimum atış süresi (ms)
  unsigned long dynamicPulseDuration;
  unsigned long currentPulseDuration;
  
  ValveControl &valve = valves[valveIndex];

  if (valve.mode == HEART_RHYTHM) {
    if (valve.useDynamicPulse) {
      dynamicPulseDuration = max(minPulseDuration, valve.beatInterval / 4);
      currentPulseDuration = dynamicPulseDuration;
    } else {
      currentPulseDuration = valve.manualPulseDuration;
    }

    if (currentTime - valve.lastBeatTime >= valve.beatInterval) {
      if (valve.debug) {
        Serial.print("Valf ");
        Serial.print(valveIndex + 1);
        Serial.println(" >>> BEAT! <<<");
      }
      setSolenoidDuty(valve.channel, valve.pwmValue);
      valve.lastBeatTime = currentTime;
      pulseStartTime[valveIndex] = currentTime;
      isPulsing[valveIndex] = true;
    }

    if (isPulsing[valveIndex] && (currentTime - pulseStartTime[valveIndex] >= currentPulseDuration)) {
      if (valve.debug) {
        Serial.print("Valf ");
        Serial.print(valveIndex + 1);
        Serial.println(" <<< END BEAT >>>");
      }
      setSolenoidDuty(valve.channel, 0);
      isPulsing[valveIndex] = false;
    }
  }
}

// Komut işleme fonksiyonları (set_valf_komutu, set_pwm, set_bpm vb.) öncekiyle aynı kalacak,
// sadece pin yerine channel kullanılacak ve 16 valfe uygun hale getirilecek

int16_t set_valf_komutu(int argc, char **argv) {
  if (argc >= 2) {
    int valveIndex = strtol(argv[1], NULL, 10) - 1;
    if (valveIndex >= 0 && valveIndex < NUM_VALVES) {
      if (argc == 7) {
        // valf <valf_no> <pwm> <bpm> <mod> <pdur> <pdmode>
        int pwm_deger = strtol(argv[2], NULL, 10);
        int bpm_deger = strtol(argv[3], NULL, 10);
        int pdur_deger = strtol(argv[5], NULL, 10);

        if (pwm_deger >= 0 && pwm_deger <= 4095) {
          valves[valveIndex].pwmValue = pwm_deger;
          setSolenoidDuty(valves[valveIndex].channel, pwm_deger);
          Serial.print("Valf "); Serial.print(valveIndex + 1); 
          Serial.print(" PWM değerine ayarlandı: "); Serial.println(pwm_deger);
        } else {
          Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
          Serial.println("): Geçersiz PWM değeri (0-4095 arasında olmalı).");
          return 1;
        }

        if (bpm_deger >= MIN_BPM && bpm_deger <= MAX_BPM) {
          valves[valveIndex].heartRate = bpm_deger;
          valves[valveIndex].beatInterval = 60000 / bpm_deger;
          Serial.print("Valf "); Serial.print(valveIndex + 1); 
          Serial.print(" Kalp ritmi değerine ayarlandı (BPM: "); 
          Serial.print(bpm_deger); Serial.println(")");
        } else {
          Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
          Serial.print("): Geçersiz BPM değeri ("); 
          Serial.print(MIN_BPM); Serial.print("-"); 
          Serial.print(MAX_BPM); Serial.println(" arasında olmalı).");
          return 1;
        }

        if (strcmp(argv[4], "m") == 0) {
          valves[valveIndex].mode = MANUAL_PWM;
          Serial.print("Valf "); Serial.print(valveIndex + 1); 
          Serial.println(" Manuel PWM kontrol modu seçildi.");
        } else if (strcmp(argv[4], "o") == 0) {
          valves[valveIndex].mode = HEART_RHYTHM;
          Serial.print("Valf "); Serial.print(valveIndex + 1); 
          Serial.println(" Kalp ritmi kontrol modu seçildi.");
        } else {
          Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
          Serial.println("): Geçersiz çalışma modu ('m' veya 'o').");
          return 1;
        }

        if (pdur_deger > 0) {
          valves[valveIndex].manualPulseDuration = pdur_deger;
          Serial.print("Valf "); Serial.print(valveIndex + 1); 
          Serial.print(" Manuel atış süresi ayarlandı (ms): "); 
          Serial.println(pdur_deger);
        } else {
          Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
          Serial.println("): Geçersiz manuel atış süresi.");
          return 1;
        }

        if (strcmp(argv[6], "m") == 0) {
          valves[valveIndex].useDynamicPulse = false;
          Serial.print("Valf "); Serial.print(valveIndex + 1); 
          Serial.println(" Manuel atış süresi kullanımı seçildi.");
        } else if (strcmp(argv[6], "o") == 0) {
          valves[valveIndex].useDynamicPulse = true;
          Serial.print("Valf "); Serial.print(valveIndex + 1); 
          Serial.println(" Otomatik atış süresi kullanımı seçildi.");
        } else {
          Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
          Serial.println("): Geçersiz duration modu ('m' veya 'o').");
          return 1;
        }
      } else {
        Serial.println("Kullanım: valf <valf_no> <pwm_değeri> <bpm> <mod (m|o)> <pdur> <pdmode (m|o)>");
        Serial.println("Örnek: valf 1 4095 70 o 50 m");
      }
    } else {
      Serial.print("Hata: Geçersiz valf numarası (1-");
      Serial.print(NUM_VALVES);
      Serial.println(" arasında olmalı).");
    }
  } else {
    Serial.println("Kullanım: valf <valf_no> ...");
  }
  return 1;
}
int16_t set_pwm(int argc, char **argv) {
  if (argc > 2) {
    int valveIndex = strtol(argv[1], NULL, 10) - 1;
    int pwmDeğer = strtol(argv[2], NULL, 10);
    if (valveIndex >= 0 && valveIndex < NUM_VALVES) {
      if (pwmDeğer >= 0 && pwmDeğer <= 4095) {
        valves[valveIndex].pwmValue = pwmDeğer;
        setSolenoidDuty(valves[valveIndex].channel, pwmDeğer);
        Serial.print("Valf "); Serial.print(valveIndex + 1); 
        Serial.print(" PWM değerine ayarlandı: "); Serial.println(pwmDeğer);
      } else {
        Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
        Serial.println("): Geçersiz PWM değeri (0-4095 arasında olmalı).");
      }
      if(argc > 3){
        if (strcmp(argv[3], "m") == 0) {
          valves[valveIndex].mode = MANUAL_PWM;
          Serial.print("Valf "); Serial.print(valveIndex + 1); 
          Serial.println(" Manuel PWM kontrol modu seçildi.");
        } else if (strcmp(argv[3], "o") == 0) {
          valves[valveIndex].mode = HEART_RHYTHM;
          Serial.print("Valf "); Serial.print(valveIndex + 1); 
          Serial.println(" Kalp ritmi kontrol modu seçildi.");
        } else {
          Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
          Serial.println("): Geçersiz çalışma modu ('m' veya 'o').");
          return 1;
        }
      }
    } else {
      Serial.print("Hata: Geçersiz valf numarası (1-"); 
      Serial.print(NUM_VALVES); Serial.println(" arasında olmalı).");
    }
  } else {
    Serial.println("Kullanım: pwm <valf_no> <duty_cycle> (0-4095) [mod]");
  }
  return 1;
}

int16_t set_bpm(int argc, char **argv) {
  if (argc > 2) {
    int valveIndex = strtol(argv[1], NULL, 10) - 1;
    int bpmDeğer = strtol(argv[2], NULL, 10);
    if (valveIndex >= 0 && valveIndex < NUM_VALVES) {
      if (bpmDeğer >= MIN_BPM && bpmDeğer <= MAX_BPM) {
        valves[valveIndex].heartRate = bpmDeğer;
        valves[valveIndex].beatInterval = 60000 / bpmDeğer;
        valves[valveIndex].mode = HEART_RHYTHM;
        Serial.print("Valf "); Serial.print(valveIndex + 1); 
        Serial.print(" kalp ritmi modunda (BPM: "); 
        Serial.print(bpmDeğer); Serial.println(").");
      } else {
        Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
        Serial.print("): BPM değeri "); Serial.print(MIN_BPM); 
        Serial.print(" ile "); Serial.print(MAX_BPM); 
        Serial.println(" arasında olmalıdır.");
      }
    } else {
      Serial.print("Hata: Geçersiz valf numarası (1-"); 
      Serial.print(NUM_VALVES); Serial.println(" arasında olmalı).");
    }
  } else {
    Serial.println("Kullanım: bpm <valf_no> <atış_sayısı_dakikada> (40-140)");
  }
  return 1;
}

int16_t set_mode(int argc, char **argv) {
  if (argc > 2) {
    int valveIndex = strtol(argv[1], NULL, 10) - 1;
    if (valveIndex >= 0 && valveIndex < NUM_VALVES) {
      if (strcmp(argv[2], "m") == 0) {
        valves[valveIndex].mode = MANUAL_PWM;
        Serial.print("Valf "); Serial.print(valveIndex + 1); 
        Serial.println(" manuel PWM moduna geçti.");
      } else if (strcmp(argv[2], "o") == 0) {
        valves[valveIndex].mode = HEART_RHYTHM;
        Serial.print("Valf "); Serial.print(valveIndex + 1); 
        Serial.println(" kalp ritmi moduna geçti.");
      } else {
        Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
        Serial.println("): Geçersiz mod ('m' veya 'o').");
      }
    } else {
      Serial.print("Hata: Geçersiz valf numarası (1-"); 
      Serial.print(NUM_VALVES); Serial.println(" arasında olmalı).");
    }
  } else {
    Serial.println("Kullanım: mode <valf_no> <m|o>");
  }
  return 1;
}

int16_t set_pulse_duration(int argc, char **argv) {
  if (argc > 2) {
    int valveIndex = strtol(argv[1], NULL, 10) - 1;
    int duration = strtol(argv[2], NULL, 10);
    if (valveIndex >= 0 && valveIndex < NUM_VALVES) {
      if (duration > 0) {
        valves[valveIndex].manualPulseDuration = duration;
        Serial.print("Valf "); Serial.print(valveIndex + 1); 
        Serial.print(" manuel atış süresi (ms): "); 
        Serial.println(valves[valveIndex].manualPulseDuration);
      } else {
        Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
        Serial.println("): Geçersiz manuel atış süresi.");
      }
    } else {
      Serial.print("Hata: Geçersiz valf numarası (1-"); 
      Serial.print(NUM_VALVES); Serial.println(" arasında olmalı).");
    }
  } else {
    Serial.println("Kullanım: pdur <valf_no> <süre_milisaniye>");
  }
  return 1;
}

int16_t set_pulse_mode(int argc, char **argv) {
  if (argc > 2) {
    int valveIndex = strtol(argv[1], NULL, 10) - 1;
    if (valveIndex >= 0 && valveIndex < NUM_VALVES) {
      if (strcmp(argv[2], "o") == 0) {
        valves[valveIndex].useDynamicPulse = true;
        Serial.print("Valf "); Serial.print(valveIndex + 1); 
        Serial.println(" dinamik atış süresi kullanıyor.");
      } else if (strcmp(argv[2], "m") == 0) {
        valves[valveIndex].useDynamicPulse = false;
        Serial.print("Valf "); Serial.print(valveIndex + 1); 
        Serial.print(" manuel atış süresi kullanıyor (süre: "); 
        Serial.print(valves[valveIndex].manualPulseDuration); 
        Serial.println(" ms).");
      } else {
        Serial.print("Hata (Valf "); Serial.print(valveIndex + 1); 
        Serial.println("): Geçersiz mod ('m' veya 'o').");
      }
    } else {
      Serial.print("Hata: Geçersiz valf numarası (1-"); 
      Serial.print(NUM_VALVES); Serial.println(" arasında olmalı).");
    }
  } else {
    Serial.println("Kullanım: pdmode <valf_no> <m|o>");
  }
  return 1;
}

int16_t set_debug(int argc, char **argv) {
  if (argc > 1) {
    int valveIndex = strtol(argv[1], NULL, 10) - 1;
    int debug = strtol(argv[2], NULL, 10);
    if (valveIndex >= 0 && valveIndex < NUM_VALVES) {
      valves[valveIndex].debug = debug;
      Serial.print("Valf "); Serial.print(valveIndex + 1); 
      Serial.print(" debug modu: "); 
      Serial.println(valves[valveIndex].debug ? "Açık" : "Kapalı");
    } else {
      Serial.print("Hata: Geçersiz valf numarası (1-"); 
      Serial.print(NUM_VALVES); Serial.println(" arasında olmalı).");
    }
  } else {
    Serial.println("Kullanım: debug <valf_no> <0|1>");
  }
  return 1;
}
// Diğer komut fonksiyonları (set_pwm, set_bpm, set_mode, set_pulse_duration, set_pulse_mode, set_debug)
// benzer şekilde güncellenerek 16 valf ve PCA9685 desteği eklenebilir