#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============ MODE SELECTION ============
#define USE_WIFI_MODE false // Standalone WiFi Mode

// ============ WIFI CONFIG (Required for compilation) ============
#define WIFI_SSID "SMKN4@WIFI2"
#define WIFI_PASSWORD "Smkn4bdl@2024"
#define WIFI_SSID_BACKUP "ARSY"
#define WIFI_PASS_BACKUP "28318568"

// ============ SUPABASE CONFIG ============
#define SUPABASE_URL "https://vpnrgikmczryyryjtwai.supabase.co"
#define SUPABASE_KEY "sb_publishable_3zeX8QdVpEYZitcA8dY_kQ_t48x_krs"

// ============ PIN DEFINITIONS ============
#define PIN_SENSOR_OUT 25 
#define PIN_S2 33         
#define PIN_S3 32         

// ============ I2C PINS ============
#define PIN_SDA 21
#define PIN_SCL 22

// ============ OLED CONFIG ============
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// ============ SENSOR LOGIC (BASIC) ============
#define DEFAULT_TOLERANCE 0.02f
#define MAX_NOMINALS 20
#define SAMPLING_PERIOD 3000

#endif
