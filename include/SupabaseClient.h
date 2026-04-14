#ifndef SUPABASE_CLIENT_H
#define SUPABASE_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Config.h>

// Configuration is now loaded from Config.h

// ============ FUNCTION DECLARATIONS ============
void initWiFi();
bool isWiFiConnected();
bool saveCalibrationToSupabase(int nominal, int refR, int refG, int refB, float tolerance);
bool saveTransactionToSupabase(int nominal, int totalBalance);
bool loadCalibrationFromSupabase(void (*callback)(int, int, int, int, float));
bool loadBalanceFromSupabase(int &balance);
bool clearAllSupabase();

// ============ WIFI INITIALIZATION ============
void initWiFi(){
#if USE_WIFI_MODE
  Serial.println("\n/ === WiFi Connection (STANDALONE MODE) ===");
  WiFi.mode(WIFI_STA);

  // --- Try Primary WiFi ---
  Serial.printf("/ Connecting to (1): %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 20){
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  // --- Failover to Secondary WiFi ---
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("\n/ Primary WiFi failed. Trying Backup...");
    Serial.printf("/ Connecting to (2): %s\n", WIFI_SSID_BACKUP);
    WiFi.begin(WIFI_SSID_BACKUP, WIFI_PASS_BACKUP);
    
    attempts = 0;
    while(WiFi.status() != WL_CONNECTED && attempts < 20){
      delay(500);
      Serial.print(".");
      attempts++;
    }
  }
  
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("\n/ WiFi Connected!");
    Serial.printf("/ IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n/ WiFi Connection Failed All Retries!");
  }
#else
  Serial.println("\n/ === SERIAL BRIDGE MODE ACTIVE ===");
  Serial.println("/ WiFi Hardware Bypassed.");
#endif
}

bool isWiFiConnected(){
#if USE_WIFI_MODE
  return WiFi.status() == WL_CONNECTED;
#else
  return true; // Always true for Serial Bridge logic
#endif
}

// ============ SAVE CALIBRATION ============
bool saveCalibrationToSupabase(int nominal, int refR, int refG, int refB, float tolerance){
#if USE_WIFI_MODE
  if(!isWiFiConnected()){
    Serial.println("/ ERROR: WiFi not connected");
    return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/calibrations";
  
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("apikey", SUPABASE_KEY);
  
  // re-use a static document to reduce heap churn
  static DynamicJsonDocument doc(256);
  doc.clear();
  doc["nominal"] = nominal;
  doc["ref_r"] = refR;
  doc["ref_g"] = refG;
  doc["ref_b"] = refB;
  doc["tolerance"] = tolerance;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.printf("/ Saving to Supabase: %s\n", jsonString.c_str());
  
  int httpCode = http.POST(jsonString);
  yield();
  
  if(httpCode == 201 || httpCode == 200){
    Serial.println("/ Calibration saved to Supabase!");
    http.end();
    return true;
  } else {
    Serial.printf("/ ERROR: HTTP %d\n", httpCode);
    Serial.println(http.getString());
    http.end();
    return false;
  }
#else
  // SERIAL BRIDGE PROTOCOL
  Serial.printf("{\"cmd\": \"save_cal\", \"nominal\": %d, \"ref_r\": %d, \"ref_g\": %d, \"ref_b\": %d, \"tolerance\": %.4f}\n",
                nominal, refR, refG, refB, tolerance);
  return true;
#endif
}

// ============ SAVE TRANSACTION ============
bool saveTransactionToSupabase(int nominal, int totalBalance){
#if USE_WIFI_MODE
  if(!isWiFiConnected()){
    Serial.println("/ ERROR: WiFi not connected");
    return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/transactions";
  
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("apikey", SUPABASE_KEY);
  
  static DynamicJsonDocument doc(256);
  doc.clear();
  doc["nominal"] = nominal;
  doc["total_balance"] = totalBalance;
  doc["detected_at"] = "now()";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpCode = http.POST(jsonString);
  yield();
  
  if(httpCode == 201 || httpCode == 200){
    Serial.printf("/ Transaction saved: Rp %d added, Total: Rp %d\n", nominal, totalBalance);
    http.end();
    return true;
  } else {
    Serial.printf("/ ERROR: HTTP %d\n", httpCode);
    http.end();
    return false;
  }
#else
  // SERIAL BRIDGE PROTOCOL
  Serial.printf("{\"cmd\": \"save_tr\", \"nominal\": %d, \"total\": %d}\n", nominal, totalBalance);
  return true;
#endif
}

// ============ LOAD CALIBRATIONS ============
bool loadCalibrationFromSupabase(void (*callback)(int, int, int, int, float)){
#if USE_WIFI_MODE
  if(!isWiFiConnected()){
    Serial.println("/ ERROR: WiFi not connected");
    return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/calibrations?select=*";
  
  http.begin(client, url);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("apikey", SUPABASE_KEY);
  
  int httpCode = http.GET();
  yield();
  
  if(httpCode == 200){
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);
    
    if(doc.is<JsonArray>()){
      JsonArray arr = doc.as<JsonArray>();
      Serial.printf("/ Loaded %d calibrations from Supabase\n", arr.size());
      
      for(JsonObject item : arr){
        int nominal = item["nominal"] | 0;
        int refR = item["ref_r"] | 0;
        int refG = item["ref_g"] | 0;
        int refB = item["ref_b"] | 0;
        float tolerance = item["tolerance"] | 0.0f;
        
        if(nominal > 0){
          callback(nominal, refR, refG, refB, tolerance);
        }
        yield();
      }
      http.end();
      return true;
    }
  } else {
    Serial.printf("/ ERROR: HTTP %d\n", httpCode);
  }
  
  http.end();
  return false;
#else
  // Request via Bridge
  Serial.println("{\"cmd\": \"get_cal\"}");
  return true;
#endif
}

// ============ LOAD BALANCE ============
bool loadBalanceFromSupabase(int &balance){
#if USE_WIFI_MODE
  if(!isWiFiConnected()){
    Serial.println("/ ERROR: WiFi not connected");
    return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/transactions?select=total_balance&order=id.desc&limit=1";
  
  http.begin(client, url);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("apikey", SUPABASE_KEY);
  
  int httpCode = http.GET();
  yield();
  
  if(httpCode == 200){
    String payload = http.getString();
    DynamicJsonDocument doc(512);
    deserializeJson(doc, payload);
    
    if(doc.is<JsonArray>() && doc.size() > 0){
      balance = doc[0]["total_balance"] | 0;
      Serial.printf("/ Loaded balance: Rp %d\n", balance);
      http.end();
      return true;
    }
  } else {
    Serial.printf("/ ERROR loading balance: HTTP %d\n", httpCode);
  }
  
  http.end();
  return false;
#else
  // Request via Bridge
  Serial.println("{\"cmd\": \"get_balance\"}");
  return true;
#endif
}

// ============ CLEAR ALL (CALIBRATIONS + TRANSACTIONS) ============
bool clearAllSupabase(){
#if USE_WIFI_MODE
  if(!isWiFiConnected()){
    Serial.println("/ ERROR: WiFi not connected");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String urlCal = String(SUPABASE_URL) + "/rest/v1/calibrations";
  http.begin(client, urlCal);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Prefer", "return=representation");

  int code1 = http.sendRequest("DELETE", "");
  if(code1 == 204 || code1 == 200){
    Serial.println("/ Calibrations cleared on Supabase");
  } else {
    Serial.printf("/ ERROR clearing calibrations: HTTP %d\n", code1);
    Serial.println(http.getString());
  }
  http.end();

  String urlTr = String(SUPABASE_URL) + "/rest/v1/transactions";
  http.begin(client, urlTr);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Prefer", "return=representation");

  int code2 = http.sendRequest("DELETE", "");
  if(code2 == 204 || code2 == 200){
    Serial.println("/ Transactions cleared on Supabase");
  } else {
    Serial.printf("/ ERROR clearing transactions: HTTP %d\n", code2);
    Serial.println(http.getString());
  }
  http.end();

  return (code1 == 204 || code1 == 200) && (code2 == 204 || code2 == 200);
#else
  Serial.println("{\"cmd\": \"clear_all\"}"); // Not standard in bridge.py but good to have
  return true;
#endif
}

#endif
