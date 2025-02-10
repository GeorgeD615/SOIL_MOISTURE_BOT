#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// WiFi настройки
const char* ssid = "wifi_name";
const char* password = "********";

// Telegram настройки
#define BOT_TOKEN "***********************"
#define CHAT_ID "*******"

// Настройки датчика влажности
#define SOIL_MOISTURE_PIN A0  
#define DRY_THRESHOLD 800   // Если значение выше - почва сухая
#define WET_THRESHOLD 150   // Если ниже - почва переувлажнена
#define SENSOR_REMOVED 1024 // Если датчик вынут из почвы

WiFiClientSecure client;

bool sensorRemovedNotified = false;  // Флаг, чтобы не спамить сообщением о вынутом датчике
int lastMoisture = -1;  // Предыдущий уровень сухости

void sendTelegramMessage(String message) {
    Serial.println("Sending Telegram message...");

    String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage";
    String data = "chat_id=" + String(CHAT_ID) + "&text=" + message + "&parse_mode=Markdown";

    client.setInsecure();  // Отключаем проверку SSL
    if (!client.connect("api.telegram.org", 443)) {
        Serial.println("⚠️ Error: Failed to connect to Telegram!");
        return;
    }

    // Отправляем HTTP-запрос
    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: api.telegram.org");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(data.length());
    client.println();
    client.print(data);

    // Читаем ответ
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        Serial.println(line);
        if (line == "\r") break;
    }
    
    client.stop();
    Serial.println("✅ Telegram message sent!");
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    // Ожидание подключения к WiFi (максимум 1 минута)
    Serial.print("Connecting to WiFi...");
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 60) {
        delay(1000);
        Serial.print(".");
        attempt++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n⚠️ Connection failed! Stopping program.");
        while (true);
    }

    Serial.println("\n✅ Connected to WiFi!");
}

void loop() {
    int moistureValue = analogRead(SOIL_MOISTURE_PIN);
    int moisturePercent = moistureValue * 100 / 1023;

    Serial.print("Soil moisture level: ");
    Serial.print(moisturePercent);
    Serial.println("%");

    // Если датчик вынут
    if (moistureValue == SENSOR_REMOVED) {
        if (!sensorRemovedNotified) {
            sendTelegramMessage("⚠️ Датчик влажности вынут из почвы!");
            sensorRemovedNotified = true;
        }
        lastMoisture = moistureValue;
        delay(10000);
        return;
    } else {
      if(sensorRemovedNotified == true){
        sendTelegramMessage("✅ Датчик влажности снова в почве!");
      }  
      sensorRemovedNotified = false;
    }

    // Если почва сухая, отправляем уведомление
    if (moistureValue > DRY_THRESHOLD) {
        sendTelegramMessage("⚠️ *Почва слишком сухая!* 🌱\nУровень сухости: " + String(moisturePercent) + "%\nПолейте растение! 🚰");
        Serial.println("Soil is too dry! Sending alert...");
        delay(10000);
        lastMoisture = moistureValue;
        return;
    }

    // Если влажность резко увеличилась на 100 и до этого была сухой
    if (lastMoisture > 0 && (lastMoisture - moistureValue) >= 100 && lastMoisture != SENSOR_REMOVED) {
        sendTelegramMessage("🙏 Спасибо, что полили почву! Ваше растение чувствует себя лучше! 🌿");
        Serial.println("Detected moisture increase! Sending thanks message...");
    }

    // Если почва слишком влажная
    if (moistureValue < WET_THRESHOLD) {
        sendTelegramMessage("⚠️ *Почва слишком влажная!* 💦\nУровень сухости: " + String(moisturePercent) + "%\nВозможно, растение залили водой!");
        Serial.println("Soil is too wet! Sending alert...");
    }

    lastMoisture = moistureValue;
    delay(10000);
}