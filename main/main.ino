#include <EEPROM.h>
#include "BluetoothSerial.h"

#define DATA_VERSION "DATA1.0"

BluetoothSerial ESP_BT;

struct WIFI_CONFIG {
    char ssid[36];
    char password[36];
    char check[10];
};
WIFI_CONFIG wifi_config;

String buffer_in;
byte val;
int addr = 0;

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

void setup() {
  Serial.println(""); 

  Serial.begin(9600);
  EEPROM.begin(1024);
  load_wifi_config();

  Serial.println("SSID: '" + String(wifi_config.ssid) + "'");
  Serial.println("PASS: '" + String(wifi_config.password) + "'");

  strcpy(wifi_config.ssid, "hogehoge");
  strcpy(wifi_config.password, "password");

  save_wifi_config();
  
  ESP_BT.begin("home-weather-station");
  Serial.println("Bluetooth Device is Ready to Pair");
  
}


void loop() {
  Serial.println("Hello World!!");
  delay(1000);
}
