#include <Arduino.h>
#include <EEPROM.h>
#include "BluetoothSerial.h"
#include "WiFi.h"

#define DATA_VERSION "DATA1.0"
#define WIFI_CONFIG_UPDATE 0
#define WIFI_CONFIG_TIMEOUT 30
#define WIND_SPD_PIN 14

BluetoothSerial ESP_BT;

struct WIFI_CONFIG {
    char ssid[36];
    char password[36];
    char check[10];
};
WIFI_CONFIG wifi_config;

volatile unsigned long timeSinceLastTick = 0;
volatile unsigned long lastTick = 0;

float windSpeed = 0.0;

void load_wifi_config() {
  EEPROM.get<WIFI_CONFIG>(0, wifi_config);
  if (strcmp(wifi_config.check, DATA_VERSION)) {
    strcpy(wifi_config.ssid, "");
    strcpy(wifi_config.password, "");
  }
}

void save_wifi_config() {
  strcpy(wifi_config.check, DATA_VERSION);
  EEPROM.put<WIFI_CONFIG>(0, wifi_config);
  EEPROM.commit();
}

void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_config.ssid, wifi_config.password);
  Serial.print("Wifi connecting to ");
  Serial.print(String(wifi_config.ssid) + " .");

  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println(" connected.");
  Serial.println("Local IP: " + WiFi.localIP().toString());
}

void wifi_config_update() {
  unsigned int startMillis;
  String buffer_in;
  String tmp;
  
  Serial.println("Waiting for wifi config updates " + String(WIFI_CONFIG_TIMEOUT) + " seconds.");
  ESP_BT.begin("home-weather-station");
  Serial.println("Bluetooth device is ready to pair.");
  
  startMillis = millis();
  while(true) {
    if (millis() - startMillis >= (WIFI_CONFIG_TIMEOUT * 1000)) {
      Serial.println(String(WIFI_CONFIG_TIMEOUT) + " seconds over");
      break;
    }

    if (ESP_BT.available()) {
      buffer_in = ESP_BT.readStringUntil('\n');
      buffer_in.trim();
      Serial.println("Received:");
      Serial.println(buffer_in);

      if (buffer_in.startsWith("SSID:")) {
        tmp = buffer_in.substring(5);
        tmp.toCharArray(wifi_config.ssid, tmp.length()+1);
        ESP_BT.println("Set SSID: " + tmp);
        Serial.println("Set SSID: " + tmp);
        save_wifi_config();
      } else if (buffer_in.startsWith("PASS:")) {
        tmp = buffer_in.substring(5);
        tmp.toCharArray(wifi_config.password, tmp.length()+1);
        ESP_BT.println("Set PASS: " + tmp);
        Serial.println("Set PASS: " + tmp);
        save_wifi_config();
      } else {
        ESP_BT.println("Invalid command: " + buffer_in);
        Serial.println("Invalid command: " + buffer_in);
      }
    }
    
    delay(20);
  }

  ESP_BT.disconnect();
  delay(200);
  ESP_BT.end();
}

void wind_tick() {
  timeSinceLastTick = millis() - lastTick;
  lastTick = millis();
}

void setup() {
  Serial.begin(9600);
  EEPROM.begin(1024);
  
  Serial.println("=== Home Weather Station ===");
  
  load_wifi_config();
  Serial.println("Loaded wifi config:");
  Serial.println("SSID: '" + String(wifi_config.ssid) + "'");
  Serial.println("PASS: '" + String(wifi_config.password) + "'");
  
  if (WIFI_CONFIG_UPDATE) wifi_config_update();
  wifi_connect();

  pinMode(WIND_SPD_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WIND_SPD_PIN), wind_tick, RISING);
}

void loop() {
  // reconnect wifi
  if (WiFi.status() == WL_DISCONNECTED) {
    wifi_connect();
  }
  delay(1000);

  // Windspeed calculation, in mph. timeSinceLastTick gets updated by an
  //  interrupt when ticks come in from the wind speed sensor.
  if (timeSinceLastTick != 0) windSpeed = 1000.0/timeSinceLastTick;
  Serial.print("Windspeed: ");
  Serial.print(windSpeed*1.492);
  Serial.println(" mph");
}