#include <Arduino.h>
#include <EEPROM.h>
#include "BluetoothSerial.h"
#include "WiFi.h"
#include "Ambient.h"

#define DATA_VERSION "DATA1.0"
#define WIFI_CONFIG_UPDATE 1
#define WIFI_CONFIG_TIMEOUT 15
#define WIND_SPD_PIN 14
#define RAIN_PIN 15
#define WIND_DIR_PIN 35
#define S_IN_DAY 86400
#define S_IN_HR 3600
#define NO_RAIN_SAMPLES 2000
volatile long rainTickList[NO_RAIN_SAMPLES];
volatile int rainTickIndex = 0;
volatile int rainTicks = 0;
int rainLastDay = 0;
int rainLastHour = 0;
int rainLastHourStart = 0;
int rainLastDayStart = 0;
long secsClock = 0;
volatile unsigned long timeSinceLastTick = 0;
volatile unsigned long lastTick = 0;

float windSpeed = 0.0;
String windDir = "";

BluetoothSerial ESP_BT;

struct WIFI_CONFIG
{
  char ssid[32];
  char password[32];
  int  cid;
  char writekey[32];
  char check[10];
};
WIFI_CONFIG wifi_config;

WiFiClient client;
Ambient ambient;

void load_wifi_config()
{
  EEPROM.get<WIFI_CONFIG>(0, wifi_config);
  if (strcmp(wifi_config.check, DATA_VERSION))
  {
    strcpy(wifi_config.ssid, "");
    strcpy(wifi_config.password, "");
    wifi_config.cid = 0;
    strcpy(wifi_config.writekey, "");
  }
}

void save_wifi_config()
{
  strcpy(wifi_config.check, DATA_VERSION);
  EEPROM.put<WIFI_CONFIG>(0, wifi_config);
  EEPROM.commit();
}

void wifi_connect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_config.ssid, wifi_config.password);
  Serial.print("Wifi connecting to ");
  Serial.print(String(wifi_config.ssid) + " .");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println(" connected.");
  Serial.println("Local IP: " + WiFi.localIP().toString());
}

void wifi_config_update()
{
  unsigned int startMillis;
  String buffer_in;
  String tmp;

  Serial.println("Waiting for wifi config updates " + String(WIFI_CONFIG_TIMEOUT) + " seconds.");
  ESP_BT.begin("home-weather-station");
  Serial.println("Bluetooth device is ready to pair.");

  startMillis = millis();
  while (true)
  {
    if (millis() - startMillis >= (WIFI_CONFIG_TIMEOUT * 1000))
    {
      Serial.println(String(WIFI_CONFIG_TIMEOUT) + " seconds over");
      break;
    }

    if (ESP_BT.available())
    {
      buffer_in = ESP_BT.readStringUntil('\n');
      buffer_in.trim();

      if (buffer_in.startsWith("SSID:"))
      {
        tmp = buffer_in.substring(5);
        tmp.toCharArray(wifi_config.ssid, tmp.length() + 1);
        ESP_BT.println("Set SSID: " + tmp);
        Serial.println("Set SSID: " + tmp);
        save_wifi_config();
        startMillis = millis();
      }
      else if (buffer_in.startsWith("PASS:"))
      {
        tmp = buffer_in.substring(5);
        tmp.toCharArray(wifi_config.password, tmp.length() + 1);
        ESP_BT.println("Set PASS: " + tmp);
        Serial.println("Set PASS: " + tmp);
        save_wifi_config();
        startMillis = millis();
      }
      else if (buffer_in.startsWith("CID:"))
      {
        tmp = buffer_in.substring(4);
        wifi_config.cid = tmp.toInt();
        ESP_BT.println("Set CID: " + String(tmp));
        Serial.println("Set CID: " + String(tmp));
        save_wifi_config();
        startMillis = millis();
      }
      else if (buffer_in.startsWith("WRITEKEY:"))
      {
        tmp = buffer_in.substring(9);
        tmp.toCharArray(wifi_config.writekey, tmp.length() + 1);
        ESP_BT.println("Set WRITEKEY: " + tmp);
        Serial.println("Set WRITEKEY: " + tmp);
        save_wifi_config();
        startMillis = millis();
      }
      else
      {
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

void wind_tick()
{
  timeSinceLastTick = millis() - lastTick;
  lastTick = millis();
}

void rain_tick()
{
  rainTickList[rainTickIndex++] = secsClock;
  if (rainTickIndex == NO_RAIN_SAMPLES)
    rainTickIndex = 0;
  rainTicks++;
}

// For the purposes of this calculation, 0deg is when the wind vane
//  is pointed at the anemometer. The angle increases in a clockwise
//  manner from there.
void windDirCalc(int vin)
{
  if (vin < 150)
    windDir = "112.5";
  else if (vin < 220)
    windDir = "67.5";
  else if (vin < 300)
    windDir = "90";
  else if (vin < 400)
    windDir = "157.5";
  else if (vin < 700)
    windDir = "135";
  else if (vin < 900)
    windDir = "202.5";
  else if (vin < 1100)
    windDir = "180";
  else if (vin < 1600)
    windDir = "22.5";
  else if (vin < 1800)
    windDir = "45";
  else if (vin < 2300)
    windDir = "247.5";
  else if (vin < 2500)
    windDir = "225";
  else if (vin < 2800)
    windDir = "337.5";
  else if (vin < 3100)
    windDir = "0";
  else if (vin < 3300)
    windDir = "292.5";
  else if (vin < 3600)
    windDir = "315";
  else if (vin < 4000)
    windDir = "270";
  else
    windDir = "0";
}

void setup()
{
  Serial.begin(9600);
  EEPROM.begin(1024);

  Serial.println("=== Home Weather Station ===");

  load_wifi_config();
  Serial.println("Loaded wifi config:");
  Serial.println("SSID: '" + String(wifi_config.ssid) + "'");
  Serial.println("PASS: '" + String(wifi_config.password) + "'");
  Serial.println("CID: '" + String(wifi_config.cid) + "'");
  Serial.println("WRITEKEY: '" + String(wifi_config.writekey) + "'");

  if (WIFI_CONFIG_UPDATE)
    wifi_config_update();
  wifi_connect();

  pinMode(WIND_SPD_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WIND_SPD_PIN), wind_tick, RISING);

  pinMode(RAIN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RAIN_PIN), rain_tick, RISING);
  for (int i = 0; i < NO_RAIN_SAMPLES; i++)
    rainTickList[i] = 0;

  ambient.begin(wifi_config.cid, wifi_config.writekey, &client);
}

void loop()
{
  static unsigned long outLoopTimer = 0;
  static unsigned long dataUploadTimer = 0;
  static unsigned long clockTimer = 0;
  static unsigned long tempMSClock = 0;

  tempMSClock += millis() - clockTimer;
  clockTimer = millis();
  while (tempMSClock >= 1000)
  {
    secsClock++;
    tempMSClock -= 1000;
  }

  if (millis() - outLoopTimer >= 2000)
  {
    outLoopTimer = millis();

    // Windspeed calculation, in m/s. timeSinceLastTick gets updated by an
    //  interrupt when ticks come in from the wind speed sensor.
    if ((millis() - lastTick) > 60000) {
      windSpeed = 0;
    } else if (timeSinceLastTick != 0) {
      windSpeed = ((1000.0 / timeSinceLastTick) * 2.4 * 1000.0) / 3600.0;
    }
    Serial.print("Windspeed: ");
    Serial.print(windSpeed);
    Serial.println(" m/s");

    // Calculate the wind direction and display it as a string.
    Serial.print("Wind dir: ");
    windDirCalc(analogRead(WIND_DIR_PIN));
    Serial.println(windDir);

    // Calculate and display rainfall totals.
    Serial.print("Rainfall last hour: ");
    Serial.println(float(rainLastHour) * 0.2794, 3);
    Serial.print("Rainfall last day: ");
    Serial.println(float(rainLastDay) * 0.2794, 3);
    Serial.print("Rainfall to date: ");
    Serial.println(float(rainTicks) * 0.2794, 3);

    // Calculate the amount of rain in the last day and hour.
    rainLastHour = 0;
    rainLastDay = 0;
    // If there are any captured rain sensor ticks...
    if (rainTicks > 0)
    {
      // Start at the end of the list. rainTickIndex will always be one greater
      //  than the number of captured samples.
      int i = rainTickIndex - 1;

      // Iterate over the list and count up the number of samples that have been
      //  captured with time stamps in the last hour.
      while ((rainTickList[i] >= secsClock - S_IN_HR) && rainTickList[i] != 0)
      {
        i--;
        if (i < 0)
          i = NO_RAIN_SAMPLES - 1;
        rainLastHour++;
      }

      // Repeat the process, this time over days.
      i = rainTickIndex - 1;
      while ((rainTickList[i] >= secsClock - S_IN_DAY) && rainTickList[i] != 0)
      {
        i--;
        if (i < 0)
          i = NO_RAIN_SAMPLES - 1;
        rainLastDay++;
      }
      rainLastDayStart = i;
    }
  }

  if (millis() - dataUploadTimer >= 60000)
  {
    dataUploadTimer = millis();

    // reconnect wifi
    if (WiFi.status() == WL_DISCONNECTED)
    {
      wifi_connect();
    }

    ambient.set(1, float(rainLastHour) * 0.2794);
    ambient.set(2, float(rainLastDay) * 0.2794);
    ambient.set(3, windDir.c_str());
    ambient.set(4, windSpeed);
    ambient.send();
  }
}