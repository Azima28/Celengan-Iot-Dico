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

void optimizeSSL() {
  secured_client.setInsecure();
  secured_client.setBufferSizes(1024, 1024);
}

void sendTelegramNotification(int nominal, int totalBalance) {
  Serial.println("/ Sending Telegram notification...");
  secured_client.stop(); // Clear previous SSL session (e.g. Supabase) to free RAM
  optimizeSSL();
  
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
}

void handleTelegramMessages() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      String chat_id = String(bot.messages[i].chat_id);
      String text = bot.messages[i].text;
      String from_name = bot.messages[i].from_name;

      text.toLowerCase();
      
      if (chat_id != CHAT_ID) {
        bot.sendMessage(chat_id, "Unauthorized user :(", "");
        continue;
      }

      Serial.println("/ Telegram Msg: " + text);

      if (text == "/start") {
        String welcome = "Selamat Datang, " + from_name + "!\n";
        welcome += "Ini adalah Celengan IoT Syamil.\n\n";
        welcome += "Gunakan /debug untuk toggle sensor log.";
        bot.sendMessage(chat_id, welcome, "");
      }

      if (text == "/debug") {
        sensorDebugMode = !sensorDebugMode;
        String msg = "Debug Mode: ";
        msg += (sensorDebugMode ? "ON ✅" : "OFF ❌");
        bot.sendMessage(chat_id, msg, "");
        Serial.printf("/ Debug Mode toggled via Telegram: %s\n", sensorDebugMode ? "ON" : "OFF");
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}
