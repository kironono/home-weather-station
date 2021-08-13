#include <Arduino.h>
#include <EEPROM.h>
#include "BluetoothSerial.h"
#include "WiFi.h"

#define DATA_VERSION "DATA1.0"
#define WIFI_CONFIG_UPDATE 0
#define WIFI_CONFIG_TIMEOUT 30
#define WIND_SPD_PIN 14
#define WIND_DIR_PIN 35

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
String windDir = "";

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

// For the purposes of this calculation, 0deg is when the wind vane
//  is pointed at the anemometer. The angle increases in a clockwise
//  manner from there.
void windDirCalc(int vin) {
  if      (vin < 150) windDir = "112.5";
  else if (vin < 220) windDir = "67.5";
  else if (vin < 300) windDir = "90";
  else if (vin < 400) windDir = "157.5";
  else if (vin < 700) windDir = "135";
  else if (vin < 900) windDir = "202.5";
  else if (vin < 1100) windDir = "180";
  else if (vin < 1600) windDir = "22.5";
  else if (vin < 1800) windDir = "45";
  else if (vin < 2300) windDir = "247.5";
  else if (vin < 2500) windDir = "225";
  else if (vin < 2800) windDir = "337.5";
  else if (vin < 3100) windDir = "0";
  else if (vin < 3300) windDir = "292.5";
  else if (vin < 3600) windDir = "315";
  else if (vin < 4000) windDir = "270";
  else windDir = "0";
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

  // Calculate the wind direction and display it as a string.
  Serial.print("Wind dir: ");
  windDirCalc(analogRead(WIND_DIR_PIN));
  Serial.println(windDir);
}