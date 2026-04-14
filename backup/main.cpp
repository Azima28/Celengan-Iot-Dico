#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SupabaseClient.h>
#include <TelegramClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Global State
int Uang = 0;

// ============ PIN DEFINITIONS ============
#define sensorOut D5
#define S2 D6
#define S3 D7
#define BUZZER_PIN D8

// ============ NOMINAL DATA ============
struct NominalData {
  int nominal;
  float ratioR, ratioG, ratioB;
  float tolerance;
  bool active;
};

NominalData nominals[20]; 
int nominalCount = 0;
bool isWaitingForClear = false; // logic to prevent double counting

// Calibration Config
bool calibrationMode = false;
bool sensorDebugMode = false;

// Forward Declarations
void updateDisplay();
void handleTelegramMessages(); // New function from TelegramClient
void showSuccessDisplay(int nominal, int confidence);
int getRed();
int getGreen();
int getBlue();
float calculateDistance(float r, float g, float b, float refR, float refG, float refB);
void playSuccessTone();
void handleSerialCommand(String command);
void loadCalibrationCallback(int nominal, int refR, int refG, int refB, float tolerance);

// ============ BUZZER ============
void playSuccessTone() {
  tone(BUZZER_PIN, 1000, 100);
  delay(150);
  tone(BUZZER_PIN, 1500, 100);
}

// ============ DISTANCE (Normalized Euclidean) ============
float calculateDistance(float r, float g, float b, float refR, float refG, float refB) {
  float dr = r - refR;
  float dg = g - refG;
  float db = b - refB;
  return sqrt(dr * dr + dg * dg + db * db);
}

// ============ SETUP ============
void setup() {
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║  ESP8266 HIGH PRECISION SYSTEM v4.0    ║");
  Serial.println("║    (Ratio-Based & Dark Filter)         ║");
  Serial.println("╚════════════════════════════════════════╝");

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  } else {
    display.clearDisplay();
    display.display();
  }

  updateDisplay();
  initWiFi();
  setupTime();

  // Load Data (Callback is updated to handle conversion if needed)
  loadCalibrationFromSupabase(loadCalibrationCallback);
  loadBalanceFromSupabase(Uang);

  Serial.println("\n/ SYSTEM READY!");
}

// ============ LOOP ============
void loop() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    if (command.length() > 0) handleSerialCommand(command);
  }

  // 0. Telegram Polling (Setiap 1 Detik)
  static unsigned long lastBotCheck = 0;
  if (millis() - lastBotCheck > 1000) {
    handleTelegramMessages();
    lastBotCheck = millis();
  }

  if (calibrationMode) return;

  // 1. Baca Sensor (Dengan Jeda)
  int R = getRed();
  int G = getGreen();
  int B = getBlue();

  if (sensorDebugMode) {
    float s = (float)(R + G + B);
    if (s > 0) {
      Serial.printf("/ DEBUG: RAW[R%d G%d B%d] | RATIO[R%.3f G%.3f B%.3f] | Sum=%d\n", 
                    R, G, B, (float)R/s, (float)G/s, (float)B/s, R+G+B);
    } else {
      Serial.printf("/ DEBUG: R=%d G=%d B=%d | Sum=0 (No Light?)\n", R, G, B);
    }
  }

  // 2. Normalisasi & Proximity Guard
  // 2. Normalisasi & Proximity Guard
  float sum = (float)(R + G + B);
  
  // Logic: 
  // - In your setup, Background (Empty) is very bright (Sum around 75).
  // - Money is darker (Sum around 321).
  // So, if Sum < 120, we assume the sensor is CLEAR (seeing the background).
  if (sum < 120) { 
     if (isWaitingForClear) {
       isWaitingForClear = false;
       Serial.println("/ SENSOR: Clear (Ready for next note)");
     }
     yield(); return; 
  }

  // Prevent double counting: skip detection if a note is still in front of sensor
  if (isWaitingForClear) {
    if (sensorDebugMode) {
      static unsigned long lastWaitMsg = 0;
      if (millis() - lastWaitMsg > 5000) {
        Serial.println("/ DEBUG: Still waiting for sensor to be CLEAR (Sum > 120)");
        lastWaitMsg = millis();
      }
    }
    yield(); return;
  }
  
  // Terlalu terang / glitch
  // if (sum < 30) { yield(); return; }

  float currentRatioR = (float)R / sum;
  float currentRatioG = (float)G / sum;
  float currentRatioB = (float)B / sum;

  // 3. Deteksi
  if (sensorDebugMode && nominalCount == 0) {
      Serial.println("/ DEBUG: No nominals to compare. Use /cal or check Supabase.");
  }

  for (int i = 0; i < nominalCount; i++) {
    float distance = calculateDistance(currentRatioR, currentRatioG, currentRatioB, 
                                       nominals[i].ratioR, nominals[i].ratioG, nominals[i].ratioB);

    if (sensorDebugMode) {
      Serial.printf("/ DEBUG: Checking Rp %d | Dist: %.4f | Tol: %.4f\n", 
                    nominals[i].nominal, distance, nominals[i].tolerance);
    }

    if (distance < nominals[i].tolerance) {
      Serial.printf("/ DETECTED: Rp %d (Dist: %.4f)\n", nominals[i].nominal, distance);
      
      int confidence = (int)((1.0f - (distance / nominals[i].tolerance)) * 100.0f);
      if (confidence < 0) confidence = 0;

      showSuccessDisplay(nominals[i].nominal, confidence);
      playSuccessTone();

      delay(2000);

      Uang += nominals[i].nominal;
      if (isWiFiConnected()) {
        saveTransactionToSupabase(nominals[i].nominal, Uang);
        sendTelegramNotification(nominals[i].nominal, Uang);
      }
      updateDisplay();
      isWaitingForClear = true; // Set flag: must wait for background to clear
      break;
    }
  }

  yield();
}

// ============ DISPLAY ============
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 20); 
  display.println(F("Nabung Yuk"));
  display.display();
}

void showSuccessDisplay(int nominal, int confidence) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 5); display.println(F("Terima"));
  display.setCursor(5, 25); display.println(F("Kasih!"));
  display.setTextSize(1);
  String msg = "Rp " + formatCurrency(nominal);
  display.setCursor(5, 50); display.println(msg.c_str());
  display.display();
}

// ============ SENSOR ============
int getRed() {
  digitalWrite(S2, LOW); digitalWrite(S3, LOW);
  delay(20); // Settling delay (matching Dico pattern but slightly longer for ESP8266)
  int val = pulseIn(sensorOut, LOW, 100000);
  return (val == 0) ? 9999 : val;
}

int getGreen() {
  digitalWrite(S2, HIGH); digitalWrite(S3, HIGH);
  delay(20); // Settling delay
  int val = pulseIn(sensorOut, LOW, 100000);
  return (val == 0) ? 9999 : val;
}

int getBlue() {
  digitalWrite(S2, LOW); digitalWrite(S3, HIGH);
  delay(20); // Settling delay
  int val = pulseIn(sensorOut, LOW, 100000);
  return (val == 0) ? 9999 : val;
}

// ============ SERIAL CMD ============
void handleSerialCommand(String command) {
  Serial.println("/ CMD: " + command);
  command.toLowerCase();
  
  if (command.startsWith("/cal")) {
    int spaceIndex = command.indexOf(' ');
    int nominal = (spaceIndex != -1) ? command.substring(spaceIndex + 1).toInt() : 0;
    
    if (nominal > 0) {
      calibrationMode = true;
      Serial.printf("/ START ADVANCED CALIBRATION: Rp %d\n", nominal);
      for (int i = 3; i >= 1; i--) { Serial.printf("/ %d...\n", i); delay(1000); }
      
      Serial.println("/ SAMPLING (3 sec - Dark Area Filtering)...");
      
      float sumRatioR = 0, sumRatioG = 0, sumRatioB = 0;
      int validSamples = 0;
      unsigned long startTime = millis();
      
      while (millis() - startTime < 3000) {
        int r = getRed(); delay(50);
        int g = getGreen(); delay(50);
        int b = getBlue(); delay(50);
        
        int totalPeriod = r + g + b;
        // DARK FILTER: Ignore if color is too dark (period too high)
        // High period (> 1500) typically means dark/black part of note
        if (totalPeriod < 1500 && totalPeriod > 50) { 
           float s = (float)totalPeriod;
           sumRatioR += (float)r / s;
           sumRatioG += (float)g / s;
           sumRatioB += (float)b / s;
           validSamples++;
           if (validSamples % 5 == 0) Serial.print(".");
        } else {
           // Skip sample
        }
      }
      
      if (validSamples > 0) {
        float avgR = sumRatioR / validSamples;
        float avgG = sumRatioG / validSamples;
        float avgB = sumRatioB / validSamples;
        
        Serial.printf("\n/ Avg Signature: R%.3f G%.3f B%.3f (%d samples)\n", avgR, avgG, avgB, validSamples);
        float tolerance = 0.07f; // Set to a reasonable balance
        
        if (nominalCount < 20) {
          nominals[nominalCount] = {nominal, avgR, avgG, avgB, tolerance, true};
          nominalCount++;
          
          // Since Supabase might expect integers, we send scaled values (* 1000)
          if (isWiFiConnected()) {
             saveCalibrationToSupabase(nominal, (int)(avgR * 1000), (int)(avgG * 1000), (int)(avgB * 1000), tolerance * 1000);
          }
          playSuccessTone();
          Serial.println("/ Advanced Calibration Saved Locally!");
        }
      } else {
        Serial.println("/ FAILED: No valid samples (Too dark or no note detected)");
      }
      calibrationMode = false;
    }
  } else if (command == "/list") {
    Serial.println("/ Current Signatures:");
    for (int i = 0; i < nominalCount; i++) {
        Serial.printf("  [%d] Rp %-6d | R%.3f G%.3f B%.3f | Tol: %.3f\n", 
                      i, nominals[i].nominal, nominals[i].ratioR, nominals[i].ratioG, nominals[i].ratioB, nominals[i].tolerance);
    }
  } else if (command == "/clear") {
    nominalCount = 0;
    Serial.println("/ Locally cleared.");
  } else if (command == "/debug") {
    sensorDebugMode = !sensorDebugMode;
    Serial.printf("/ Debug Mode: %s\n", sensorDebugMode ? "ON" : "OFF");
  }
}

// ============ CALLBACK ============
void loadCalibrationCallback(int nominal, int refR, int refG, int refB, float tolerance) {
  if (nominalCount < 20) {
    // Convert back from scaled integers if needed
    // If Suapabase stores raw RGB, we should calculate ratios. 
    // If it stores scaled ratios (our new system), we divide by 1000.
    float r, g, b, t;
    if (refR > 1000) { // Likely raw RGB
       float s = (float)(refR + refG + refB);
       r = (float)refR / s; g = (float)refG / s; b = (float)refB / s; t = 0.04f;
    } else { // Likely scaled ratios
       r = (float)refR / 1000.0f; g = (float)refG / 1000.0f; b = (float)refB / 1000.0f; t = tolerance / 1000.0f;
       if (t <= 0) t = 0.04f;
    }
    nominals[nominalCount] = {nominal, r, g, b, t, true};
    nominalCount++;
  }
}
