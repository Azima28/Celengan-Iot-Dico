#ifndef TELEGRAM_CLIENT_H
#define TELEGRAM_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <time.h>

// ============ TELEGRAM CONFIG ============
#define BOT_TOKEN "8690059316:AAFgC6wj0lFZEPh-M_NRoVtF6H3hXnLe8bw"
#define CHAT_ID "7889066155"

// ============ WIFI CLIENT ============
extern WiFiClientSecure secured_client;
extern UniversalTelegramBot bot;

// ============ TIME CONFIG ============
extern const char* ntp_server;
extern const long gmt_offset_sec;
extern const int daylight_offset_sec;

extern bool sensorDebugMode;
void optimizeSSL();
void handleTelegramMessages();
void setupTime();
String getFormattedTime();
String formatCurrency(int value);
void sendTelegramNotification(int nominal, int totalBalance);

#endif
