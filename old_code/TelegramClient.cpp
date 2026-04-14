#include <TelegramClient.h>

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

const char* ntp_server = "pool.ntp.org";
const long gmt_offset_sec = 7 * 3600; // GMT+7 (WIB)
const int daylight_offset_sec = 0;

void setupTime() {
  Serial.println("/ Syncing time with NTP...");
  configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
  
  time_t now = time(nullptr);
  int retry = 0;
  while (now < 8 * 3600 * 2 && retry < 10) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    retry++;
  }
  Serial.println("\n/ Time synced: " + String(ctime(&now)));
}

String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%H:%M %d/%m/%Y", timeinfo);
  return String(buffer);
}

String formatCurrency(int value) {
  String valStr = String(value);
  int len = valStr.length();
  if (len <= 3) return valStr;
  
  String result = "";
  int count = 0;
  for (int i = len - 1; i >= 0; i--) {
    result = valStr[i] + result;
    count++;
    if (count % 3 == 0 && i > 0) {
      result = "." + result;
    }
  }
  return result;
}

void sendTelegramNotification(int nominal, int totalBalance) {
#if USE_WIFI_MODE
  Serial.println("/ Sending Telegram notification...");
  
  // ESP32 requirement for secured client
  secured_client.setInsecure();
  
  String timestamp = getFormattedTime();
  String message = "💰 *ADA UANG MASUK!*\n\n";
  message += "━━━━━━━━━━━━━━━\n";
  message += "💵 *Nominal:* \n      Rp " + formatCurrency(nominal) + "\n\n";
  message += "📊 *Total Tabungan:* \n      Rp " + formatCurrency(totalBalance) + "\n\n";
  message += "⏰ *Waktu:* \n      " + timestamp + "\n";
  message += "━━━━━━━━━━━━━━━";
  
  if (bot.sendMessage(CHAT_ID, message, "Markdown")) {
    Serial.println("/ Telegram notification sent!");
  } else {
    Serial.println("/ Telegram notification failed!");
  }
#else
  // In Bridge Mode, Python handles Telegram automatically to save ESP32 memory/time
  // Serial.println("/ [Bridge Mode] Telegram will be sent by PC.");
#endif
}
