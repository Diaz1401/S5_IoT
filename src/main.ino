/**
 * @file main.ino
 * @brief IoT Water Quality Monitoring System for ESP32
 * 
 * This sketch implements a comprehensive water quality monitoring system that reads
 * multiple sensor values and logs them to Firebase Realtime Database. It includes
 * support for demo mode with simulated data for testing purposes.
 * 
 * @details
 * 
 * **Sensors:**
 * - Turbidity Sensor (GPIO 32): Measures water clarity in NTU units
 * - TDS Sensor (GPIO 33): Measures Total Dissolved Solids with temperature compensation
 * - pH Sensor (GPIO 34): Measures acidity/alkalinity with calibration offset and slope
 * - DS18B20 Temperature Sensor (GPIO 4): Measures water temperature via One-Wire protocol
 * 
 * **Features:**
 * - WiFi connectivity with secure SSL/TLS communication
 * - Firebase Realtime Database integration for data persistence
 * - NTP time synchronization for accurate timestamps
 * - Configurable calibration parameters for each sensor
 * - Demo mode with random data generation for testing
 * - Asynchronous Firebase operations with event-based callback handling
 * - Sensor data stored both as historical records and real-time snapshot
 * 
 * **Configuration:**
 * Requires a "secret.h" file defining:
 * - WIFI_SSID, WIFI_PASSWORD: Network credentials
 * - API_KEY, DATABASE_URL: Firebase project details
 * - USER_EMAIL, USER_PASSWORD: Firebase authentication credentials
 * - DEMO_MODE: Boolean to enable/disable demo mode (default: false)
 * 
 * **Data Structure:**
 * Sensor readings are stored in Firebase at: /sensorData/{uid}/{timestamp}
 * Real-time data snapshot is maintained at: /sensorData/{uid}/realtime
 * Each record contains: temperature, turbidity, tds, ph, and timestamp
 * 
 * **Timing:**
 * - Demo mode: Sends data every 2 seconds
 * - Normal mode: Sends data every 60 seconds
 * 
 * @see https://randomnerdtutorials.com/esp32-data-logging-firebase-realtime-database
 * 
 * @authors:
 * - Yudo Lasprianto (Initial pin setup and sensor calibration)
 * - Diaz Nuraji (Add RTDB, NTP, demo mode)
 * 
 * @date 2025
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "time.h"

/* Load secret credentials. This header file should define the following:
#define WIFI_SSID       "your-ssid"
#define WIFI_PASSWORD   "your-password"
#define API_KEY         "your-firebase-api-key"
#define DATABASE_URL    "your-firebase-database-url"
#define USER_EMAIL      "your-firebase-user-email"
#define USER_PASSWORD   "your-firebase-user-password"
const bool DEMO_MODE = false;  // Set to true for demo mode with random data
Big thanks to https://randomnerdtutorials.com/esp32-data-logging-firebase-realtime-database
*/
#include "secret.h"

// ---------- PIN SENSOR ----------
const int turbidityPin = 32;
const int tdsPin = 33;
const int pHPin = 34;

// ---------- DS18B20 ----------
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ---------- Kalibrasi ----------
float tdsCalibration = 0.3;
float phVoltageOffset = 0.0;
float phSlope = 3.0;

// ---------- NTP ----------
const char* ntpServer = "id.pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

// ---------- Firebase Objects ----------
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
DefaultNetwork network;
AsyncClient aClient(ssl_client, getNetwork(network));
RealtimeDatabase Database;

// Timer variables
unsigned long lastSendTime = 0;
const unsigned long sendInterval = DEMO_MODE ? 2000 : 60000;

// Variables
String uid;
unsigned long timestamp;

// JSON objects
object_t jsonData, obj1, obj2, obj3, obj4, obj5;
JsonWriter writer;

// Forward declarations
void processData(AsyncResult &aResult);

// ---------------------------------------------------------------------
// SENSOR FUNCTIONS
// ---------------------------------------------------------------------

float readTurbidity() {
  if (DEMO_MODE) return random(5.00, 30.00);
  int raw = analogRead(turbidityPin);
  float voltage = (raw / 4095.0) * 3.3 * (5.0 / 3.3);
  float ntu = -1234.6 * voltage + 4925;
  if (ntu < 0) ntu = 0;
  return ntu;
}

float readTDS(float temperature) {
  if (DEMO_MODE) return random(100.00, 250.00);
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(tdsPin);
    delay(5);
  }
  float adc = sum / 10.0;
  float voltage = (adc / 4095.0) * 3.3 * (5.0 / 3.3);

  float compensation = 1 + 0.02 * (temperature - 25.0);
  float compensatedVoltage = voltage / compensation;

  float tds = (133.42 * pow(compensatedVoltage, 3)
               - 255.86 * pow(compensatedVoltage, 2)
               + 857.39 * compensatedVoltage) * tdsCalibration;

  return tds;
}

float readPH() {
  if (DEMO_MODE) return random(6.5, 7.5);
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(pHPin);
    delay(5);
  }
  float adc = sum / 10.0;
  float voltage = (adc / 4095.0) * 3.3 * (5.0 / 3.3);
  float pHValue = 7 + ((2.5 - voltage + phVoltageOffset) / phSlope);
  return pHValue;
}

float readTemperature() {
  if (DEMO_MODE) return random(25.0, 32.0);
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return(0);
  }
  time(&now);
  return now;
}

// ---------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(35));
  analogReadResolution(12);
  sensors.begin();

  // =================== WIFI ===================
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi Connected!");

  // =================== NTP ===================
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // =================== FIREBASE ===================
  ssl_client.setInsecure();
  ssl_client.setTimeout(10000);
  ssl_client.setHandshakeTimeout(30);

  Serial.println("Initializing Firebase...");
  initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

void loop() {
  app.loop();

  if (app.ready()) {
    unsigned long currentTime = millis();
    if (currentTime - lastSendTime >= sendInterval) {
      lastSendTime = currentTime;

      uid = app.getUid().c_str();
      timestamp = getTime();

      if (timestamp == 0) {
        Serial.println("Failed to obtain time");
        return;
      }

      String parentPath = "/sensorData/" + uid;
      String timestampPath = parentPath + "/" + String(timestamp);

      float temperature = readTemperature();
      float turbidity = readTurbidity();
      float tds = readTDS(temperature);
      float ph = readPH();

      Serial.println("----- SENSOR DATA -----");
      Serial.println("Timestamp: " + String(timestamp));
      Serial.println("Temp: " + String(temperature));
      Serial.println("Turbidity: " + String(turbidity));
      Serial.println("TDS: " + String(tds));
      Serial.println("pH: " + String(ph));
      Serial.println("------------------------");

      // Create JSON
      writer.create(obj1, "/temperature", temperature);
      writer.create(obj2, "/turbidity", turbidity);
      writer.create(obj3, "/tds", tds);
      writer.create(obj4, "/ph", ph);
      writer.create(obj5, "/timestamp", (int)timestamp);

      writer.join(jsonData, 5, obj1, obj2, obj3, obj4, obj5);

      Database.set<object_t>(aClient, timestampPath, jsonData, processData, "RTDB_Send_Data");
      
      String realtimePath = parentPath + "/realtime";
      Database.set<object_t>(aClient, realtimePath, jsonData, processData, "RTDB_Update_Realtime");
    }
  }
}

void processData(AsyncResult &aResult) {
  // if (!aResult.isResult()) return;

  if (aResult.isEvent()) {
    Serial.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
  }

  if (aResult.isDebug()) {
    Serial.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
  }

  if (aResult.isError()) {
    Serial.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
  }

  if (aResult.available()) {
    Serial.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
  }
}
