# Celengan Dico - Configuration Summary (Old Code)

This document summarizes all important configurations, pins, and credentials extracted from the `old_code` folder.

## 1. Hardware Pinout (ESP32)
*Source: `main.cpp`*

| Component | Pin | Function |
| :--- | :--- | :--- |
| **TCS3200 sensorOut** | `25` | Frequency output |
| **TCS3200 S2** | `33` | Photo-diode type selection |
| **TCS3200 S3** | `32` | Photo-diode type selection |
| **TCS3200 S0** | `3.3V`| Frequency scaling (HIGH) |
| **TCS3200 S1** | `GND` | Frequency scaling (LOW) |
| **TCS3200 LED**| `3.3V`| Sensor LED (Always ON) |
| **I2C SDA** | `21` | OLED Data |
| **I2C SCL** | `22` | OLED Clock |

## 2. OLED Display (SSD1306)
*Source: `main.cpp`*

- **Width:** `128`
- **Height:** `64`
- **I2C Address:** `0x3C`
- **Reset Pin:** `-1` (NC)

## 3. WiFi & Supabase (Standalone WiFi Mode)
*Source: `SupabaseClient.h`*

- **Supabase URL:** `https://bivdzhwizmgyvobypzys.supabase.co`
- **Supabase Key:** `sb_publishable_Q6I1cfAm6AVi6X221_kNVA_xpXvNph4`
- **Primary SSID:** `SMKN4@WIFI2`
- **Primary Password:** `Smkn4bdl@2024`
- **Secondary SSID:** `ARSY`
- **Secondary Password:** `28318568`

## 4. Supabase & Serial (Bridge Mode)
*Source: `bridge.py`*

- **Supabase URL:** `https://vpnrgikmczryyryjtwai.supabase.co`
- **Supabase Key:** `sb_publishable_3zeX8QdVpEYZitcA8dY_kQ_t48x_krs`
- **Serial Port:** `COM3`
- **Baud Rate:** `115200`

## 5. Telegram Notifications
*Source: `TelegramClient.h` & `bridge.py`*

- **Bot Token:** `8690059316:AAFgC6wj0lFZEPh-M_NRoVtF6H3hXnLe8bw`
- **Chat ID:** `7889066155`

## 6. Sensor Logic Constants (do not use this anymore)
*Source: `main.cpp`*

- **Default Tolerance:** `0.04`
- **Dark Area Sum Threshold:** `1500`
- **Lower Brightness Limit:** `30`
- **Settling Delay:** `10ms`
- **Sampling Window:** `3000ms`
