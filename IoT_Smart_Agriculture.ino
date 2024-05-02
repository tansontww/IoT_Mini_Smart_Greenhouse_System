#define BLYNK_TEMPLATE_ID "TMPL5QnD7wJfZ"                                   // Replace with you value
#define BLYNK_TEMPLATE_NAME "IoT Mini Smart Greenhouse System"              // Replace with you value
#define BLYNK_AUTH_TOKEN "dV4CC2TKS7t0whUjNqS0prdlb6S-PVva"                 // Replace with you value
#define BLYNK_PRINT Serial

// Global variables that needs to be replaced with your values!
char ssid[] = "SOTON-IoT";                                                  // WiFi Name 
char pass[] = "Oio602npoEKo";                                               // WiFi Password 
char auth[] = "dV4CC2TKS7t0whUjNqS0prdlb6S-PVva";                           // Blynk Auth Token 
const double latitude = 50.93463;                                           // Latitude for Highfield campus, UK
const double longitude = -1.39595;                                          // Longitude for Highfield campus, UK
DeviceAddress sensor1 = { 0x28, 0x38, 0x7D, 0x81, 0xE3, 0xE1, 0x3C, 0xA };  // Address of soil temperature sensor 1
DeviceAddress sensor2 = { 0x28, 0x57, 0xFC, 0x81, 0xE3, 0xE1, 0x3C, 0xEB }; // Address of soil tempeature sensor 2

// Global Variables Controlled By Blynk (No need to change)
int ledThreshold = 1000;                          // Default effective sunlight threshold in lx
float lowerSoilTempThreshold = 15;                // Default lower soil temperature threshold in degree C
float upperSoilTempThreshold = 18;                // Default upper soil temperature threshold in degree C
int lowerSoilMoistThreshold = 70;                 // Default lower soil moisture threshold in %
int upperSoilMoistThreshold = 90;                 // Default upper soil moisture threshold in %
int scheduledHour = 6;                            // Default hour for watering
int scheduledMinute = 0;                          // Default minute for watering
float desiredLightHours = 16;                     // Default ideal light hours everyday

// Global Variables that Requires Manual Configuration (Only change if necessary)
unsigned long waterDuration = 2000;               // Default Initial morning watering duration in ms
const unsigned long adjustFactor = 250;           // Adjustment factor for morning watering routine in ms
const float safetySoilMoist = 40.0;               // Safety line for soil moisture (%)
const unsigned long safetyWaterDuration = 4000;   // Duration for safety watering (ms)

// Libraries
#include <Ticker.h>                   // For creating timers
#include <WiFi.h>                     // For Wi-Fi
#include <DallasTemperature.h>        // For DHT11
#include <BlynkSimpleEsp32.h>         // Blynk Library for ESP32
#include <Wire.h>                     // For I2C communication
#include <BH1750.h>                   // For HB1750
#include <DHT.h>                      // For DHT11
#include <OneWire.h>                  // For Soil moisture sensor
#include <algorithm>                  // For max function
#include <time.h>                     // For time function
#include <math.h>                     // For all mathematical functions
#include <HTTPClient.h>               // For HTTP request
#include <ArduinoJson.h>              // JSON parsing for processing datetime 

// GPIO pins and configurations
#define DHTPIN 15                  // For DHT11
#define MOIST_PIN 34               // For capacitive soil moisture sensor
#define ONE_WIRE_BUS 4
#define WATER_PUMP_PIN 25
#define HEAT_MAT_PIN 26
#define LED_PIN 27 
#define DHTTYPE DHT11              // DHT 11 Air temperature and moisture sensor
#define SOUND_SPEED 0.034          // SR04 Ultrasonic sensor
#define CM_TO_INCH 0.393701        // SR04 Ultrasonic sensor

// Virtual pins for blynk 
#define VPIN_AIR_HUMIDITY V0
#define VPIN_AIR_TEMPERATURE V1
#define VPIN_SOIL_MOISTURE V2
#define VPIN_SOIL_TEMPERATURE V3
#define VPIN_WATER_TANK_LEVEL V4
#define VPIN_LIGHT_SENSOR V5
#define VPIN_CURRENT_WATER_DURATION V6
#define VPIN_WATER_PUMP_STATUS V7
#define VPIN_HEAT_MAT_STATUS V8
#define VPIN_LED_GROW_LIGHT_STATUS V9
#define VPIN_LED_GROW_LIGHT_THRESHOLD V10
#define VPIN_LOWER_SOIL_TEMP_THRESHOLD V11
#define VPIN_UPPER_SOIL_TEMP_THRESHOLD V12
#define VPIN_LOWER_SOIL_MOISTURE_THRESHOLD V13
#define VPIN_UPPER_SOIL_MOISTURE_THRESHOLD V14
#define VPIN_HOUR_SLIDER V15
#define VPIN_MINUTE_SLIDER V16
#define VPIN_DERSIRED_LIGHT_HOURS V17
#define VPIN_SYSTEM_STATUS_CODE V18

// BLYNK_WRITE functions for new virtual pins
BLYNK_WRITE(VPIN_LED_GROW_LIGHT_THRESHOLD) { // LED Grow Light Threshold (%)
  ledThreshold = param.asInt();
}

BLYNK_WRITE(VPIN_LOWER_SOIL_TEMP_THRESHOLD) { // Lower Soil Temperature Threshold
  lowerSoilTempThreshold = param.asFloat();
}

BLYNK_WRITE(VPIN_UPPER_SOIL_TEMP_THRESHOLD) { // Upper Soil Temperature Threshold
  upperSoilTempThreshold = param.asFloat();
}

BLYNK_WRITE(VPIN_LOWER_SOIL_MOISTURE_THRESHOLD) { // Lower Soil Moisture Threshold
  lowerSoilMoistThreshold = param.asInt();
}

BLYNK_WRITE(VPIN_UPPER_SOIL_MOISTURE_THRESHOLD) { // Upper Soil Moisture Threshold
  upperSoilMoistThreshold = param.asInt();
}

BLYNK_WRITE(VPIN_HOUR_SLIDER) {
  scheduledHour = param.asInt(); // 0 to 23
}

BLYNK_WRITE(VPIN_MINUTE_SLIDER) {
  scheduledMinute = param.asInt(); // 0 to 59
}

BLYNK_WRITE(VPIN_DERSIRED_LIGHT_HOURS){
  desiredLightHours = param.asFloat();
}

/////////////////////////////////////////// Global Variables ////////////////////////////////////////////////////////
// Initialize the variables
unsigned long lastPumpActivation = 0; // Last time the pump was activated
float effectiveSunlightHours = 0;
float ledOnTimeHours = 0;  // Total hours the LED has been on for the current day
double sunriseHour = 0;
double sunsetHour = 0;
const int MAX_RETRY_COUNT = 3;
const int RETRY_DELAY_MS = 5000; // 5 seconds
int lastAdjustmentDate;
float globalSoilMoist = 0;
float globalSoilTemp = 0;
float globalTankLevel = 0;
float globalLightLevel = 0;
float globalAirTemp = 0;
float globalAirHumid = 0;

// Global Flags
bool isLedOn = false;
bool isHeatMatOn = false;    // State of the heat mat
bool pumpActive = false;
bool ledShouldBeOn = false; // Whether the LED should currently be on

//For SR04 Ultrasonic sensor
const int trigPin = 5; 
const int echoPin = 18;
long duration;
float distanceCm;
float distanceInch;

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;
BH1750 lightMeter;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
///////////////////////////////////////////////////////// Measurements ////////////////////////////////////////////////////////////////
float measureSoilTemp(){
  sensors.requestTemperatures();
  float tempC1 = sensors.getTempC(sensor1);
  float tempC2 = sensors.getTempC(sensor2);
  if (tempC1 == DEVICE_DISCONNECTED_C || tempC2 == DEVICE_DISCONNECTED_C) {
    sendSystemStatus("007"); // Soil temperature sensor failure
    return -999; // Error
  }
  float avgTempC = (tempC1 + tempC2) / 2;
  sendSystemStatus("000"); // All good
  return avgTempC;
}

float measureMoist(){
  int moist = analogRead(MOIST_PIN);
  if (moist < 0 || moist > 4500) { // Check if reading is within the valid range
    sendSystemStatus("008"); // Soil moisture sensor failure
    return -1; // Error
  }
  float moistPercent = ((float)(4000 - moist) / (2000)) * 100;
  moistPercent = constrain(moistPercent, 0, 100);
  sendSystemStatus("000"); // All good
  return moistPercent;
}

float measureWaterTankLevel() {
  float distance = measureDistance();
  float fullTankDistance = 2.5; // Distance when the tank is full.
  float emptyTankDistance = 17.0; // Distance when the tank is empty.
  float tankLevelPercent = (1.0 - ((distance - fullTankDistance) / (emptyTankDistance - fullTankDistance))) * 100;
  tankLevelPercent = constrain(tankLevelPercent, 0, 100);
  return tankLevelPercent;
}

float measureDistance(){
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH, 30000); // Timeout after 30000 microseconds
  if (duration == 0) {
    sendSystemStatus("006"); // Ultrasonic sensor timeout error
    return -1; // Error
  }
  distanceCm = duration * SOUND_SPEED / 2;
  if (distanceCm < 0 || distanceCm > 50) { 
    sendSystemStatus("006"); // Distance measurement error
    return -1;
  }
  sendSystemStatus("000"); // All good
  return distanceCm;
}

//////////////////////////////////////// Time /////////////////////////////////////////////////////////////////////////////////
void setupRTC() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "europe.pool.ntp.org");
  setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0", 1);
  tzset();

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    sendSystemStatus("001"); // Send error code
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  sendSystemStatus("000"); // Normal operation
}

void fetchSunriseSunset() {
  HTTPClient http;
  String serverPath = "http://api.sunrise-sunset.org/json?lat=" + String(latitude) + "&lng=" + String(longitude) + "&formatted=0";
  int httpResponseCode = 0;

  for (int retry = 0; retry < MAX_RETRY_COUNT; retry++) {
    http.begin(serverPath);
    httpResponseCode = http.GET();
       
    if (httpResponseCode > 0) {
      String response = http.getString();
      deserializeAndSetTimes(response);
      http.end();
      sendSystemStatus("000"); // Successful fetch
      return; // Exit the function as the request was successful
    } else {
      Serial.print("Error on sending GET: ");
      Serial.println(httpResponseCode);
      http.end();
      delay(RETRY_DELAY_MS); // Wait for a specified time before retrying
    }
  }
  // Handle the case where all retries failed
  Serial.println("All retries failed, using default sunrise and sunset times.");
  // Set default times or handle the error as appropriate
  sendSystemStatus("002"); // Error code for fetch failure
  sunriseHour = 6.0; // Default sunrise time
  sunsetHour = 18.0; // Default sunset time
}

void deserializeAndSetTimes(String json) {
  DynamicJsonDocument doc(2048);  // Adjusted size for safety
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("JSON deserialize failed: ");
    Serial.println(error.c_str());
    sendSystemStatus("003");
    return;
  }

  const char* sunrise = doc["results"]["sunrise"];
  const char* sunset = doc["results"]["sunset"];

  // Parse times and adjust for local time zone
  struct tm sunriseTime, sunsetTime;
  strptime(sunrise, "%Y-%m-%dT%H:%M:%S%z", &sunriseTime);
  strptime(sunset, "%Y-%m-%dT%H:%M:%S%z", &sunsetTime);

  // Assuming your timezone setup in setupRTC(), adjust the struct tm to local time if necessary
  time_t sunriseEpoch = mktime(&sunriseTime);
  time_t sunsetEpoch = mktime(&sunsetTime);

  // Adjusting from UTC to local time if needed
  localtime_r(&sunriseEpoch, &sunriseTime);
  localtime_r(&sunsetEpoch, &sunsetTime);

  sunriseHour = sunriseTime.tm_hour + (sunriseTime.tm_min / 60.0);
  sunsetHour = sunsetTime.tm_hour + (sunsetTime.tm_min / 60.0);

  Serial.print("Local Sunrise: "); Serial.println(sunriseHour);
  Serial.print("Local Sunset: "); Serial.println(sunsetHour);
}

void checkForDailyUpdate() {
  static bool fetchedToday = false;
  static bool resetDone = false;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    sendSystemStatus("001"); // Send error code as a string
    return;
  }

  // Fetch sunrise and sunset times at midnight
  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && !fetchedToday) {
    fetchSunriseSunset();
    fetchedToday = true;
    Serial.println("Sunrise and sunset times fetched.");
  } else if (timeinfo.tm_hour != 0) {
    fetchedToday = false;  // Ensure this runs once per day
  }

  // Check if current time matches the sunrise time to reset variables
  int sunriseHourOnly = int(sunriseHour);
  int sunriseMinuteOnly = int((sunriseHour - sunriseHourOnly) * 60);
  if (timeinfo.tm_hour == sunriseHourOnly && timeinfo.tm_min == sunriseMinuteOnly && !resetDone) {
    resetDailyVariables();
    resetDone = true;
    Serial.println("Daily variables reset at sunrise.");
  } else if (timeinfo.tm_hour != sunriseHourOnly || timeinfo.tm_min != sunriseMinuteOnly) {
    resetDone = false;  // Allow reset to run the next day
  }
}

void resetDailyVariables() {
  effectiveSunlightHours = 0;
  ledOnTimeHours = 0;
  Serial.println("Effective sunlight hours and LED on time hours have been reset.");
}
////////////////////////////////////// Sensor Data /////////////////////////////////////////////
void updateSensorReadings() {
  // Fetch and store environmental data from connected sensors
  globalSoilMoist = measureMoist();
  globalSoilTemp = measureSoilTemp();
  globalAirHumid = dht.readHumidity();
  globalAirTemp = dht.readTemperature();
  globalTankLevel = measureWaterTankLevel();
  globalLightLevel = lightMeter.readLightLevel();
  // Send updated readings to Blynk
  sendSensorData();

  // Log data reporting status
  Serial.println("System is reporting data.");
}

void sendSensorData() {
  // Update water pump status based on recent activity
  if (millis() - lastPumpActivation < 5000) { 
    Blynk.virtualWrite(VPIN_WATER_PUMP_STATUS, 1);
  } else {
    Blynk.virtualWrite(VPIN_WATER_PUMP_STATUS, 0);
  }
  // Send statuses of heat mat, LED grow light, and other sensor readings to Blynk
  Blynk.virtualWrite(VPIN_HEAT_MAT_STATUS, isHeatMatOn ? 1 : 0);
  Blynk.virtualWrite(VPIN_LED_GROW_LIGHT_STATUS, isLedOn ? 1 : 0);
  Blynk.virtualWrite(VPIN_SOIL_MOISTURE, globalSoilMoist);
  Blynk.virtualWrite(VPIN_SOIL_TEMPERATURE, globalSoilTemp);
  Blynk.virtualWrite(VPIN_AIR_HUMIDITY, globalAirHumid);
  Blynk.virtualWrite(VPIN_AIR_TEMPERATURE, globalAirTemp);
  Blynk.virtualWrite(VPIN_WATER_TANK_LEVEL, globalTankLevel);
  Blynk.virtualWrite(VPIN_LIGHT_SENSOR, globalLightLevel);
  Blynk.virtualWrite(VPIN_CURRENT_WATER_DURATION, waterDuration);
}

void sendSystemStatus(const String& errorCode) {
  // Use a static variable to store the last error code sent
  static String lastErrorCode = ""; 
  if (errorCode != lastErrorCode) {
    Blynk.virtualWrite(VPIN_SYSTEM_STATUS_CODE, errorCode);
    lastErrorCode = errorCode; // Update the last error code sent
    Serial.print("System Status Updated: ");
    Serial.println(errorCode);
  }
}

/////////////////////////////////////// Control Actuators ///////////////////////////////////////////
void controlHeatMat(float soilTemp) {
  if (soilTemp < lowerSoilTempThreshold && !isHeatMatOn) {
    digitalWrite(HEAT_MAT_PIN, LOW); // Turn on the heat mat
    Blynk.virtualWrite(VPIN_HEAT_MAT_STATUS, 1);
    isHeatMatOn = true;
    Serial.println("Heat mat activated.");
  } else if (soilTemp > upperSoilTempThreshold && isHeatMatOn) {
    digitalWrite(HEAT_MAT_PIN, HIGH); // Turn off the heat mat
    Blynk.virtualWrite(VPIN_HEAT_MAT_STATUS, 0);
    isHeatMatOn = false;
    Serial.println("Heat mat deactivated.");
  }
}

void controlLedGrowLight(float lightLevel, int currentHour) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    sendSystemStatus("001"); // Send error code as a string
    return;
  }
  
  // Update effective sunlight hours based on the current hour and predefined sunrise and sunset times
  if (currentHour >= sunriseHour && currentHour <= sunsetHour && lightLevel > ledThreshold) {
    effectiveSunlightHours += 1.0 / 60.0; // Assuming function is called every minute
  }

  float additionalLightNeeded = desiredLightHours - effectiveSunlightHours;

  // Control the LED based on the need for additional light and the current hour relative to sunrise and sunset
  if ((currentHour < sunriseHour || currentHour > sunsetHour) && additionalLightNeeded > 0) {
    if (!isLedOn) {
      digitalWrite(LED_PIN, LOW);  // Turn on the LED
      Blynk.virtualWrite(VPIN_LED_GROW_LIGHT_STATUS, 1);
      isLedOn = true;
    }
    ledOnTimeHours += 1.0 / 60.0; // Track how long the LED has been on during the current day
  } else {
    if (isLedOn) {
      digitalWrite(LED_PIN, HIGH); // Turn off LED
      Blynk.virtualWrite(VPIN_LED_GROW_LIGHT_STATUS, 0);
      isLedOn = false;
    }
  }

  // Automatically turn off the LED once the required light hours are met for the day
  if ((effectiveSunlightHours + ledOnTimeHours) >= desiredLightHours && isLedOn) {
    digitalWrite(LED_PIN, HIGH); // Turn off LED
    Blynk.virtualWrite(VPIN_LED_GROW_LIGHT_STATUS, 0);
    isLedOn = false;
  }
  Blynk.virtualWrite(VPIN_LED_GROW_LIGHT_STATUS, isLedOn ? 1 : 0);
}

void stopWaterPump() {
    digitalWrite(WATER_PUMP_PIN, HIGH); // Deactivate water pump
    Blynk.virtualWrite(VPIN_WATER_PUMP_STATUS, 0);
    pumpActive = false;
    Serial.println("Watering completed.");
}

void controlWaterPump(float soilMoist, float tankLevel) {
  // Water pump duration logic
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    sendSystemStatus("001"); // Send error code as a string
    return;
  }
  int currentDate = timeinfo.tm_year * 10000 + timeinfo.tm_mon * 100 + timeinfo.tm_mday;

  // Water pump logic with date check
  if (soilMoist < lowerSoilMoistThreshold && currentDate != lastAdjustmentDate) {
    waterDuration += adjustFactor; // Increase by adjustment factor
    lastAdjustmentDate = currentDate; // Update the last adjustment date
  } else if (soilMoist > upperSoilMoistThreshold && waterDuration > 1000) {
    // Decrease by adjustment factor but not below 1000 ms
    waterDuration = max(waterDuration - adjustFactor, static_cast<long unsigned int>(1000)); 
    lastAdjustmentDate = currentDate; // Update the last adjustment date
  }

  if (soilMoist < safetySoilMoist && tankLevel > 10 && !pumpActive) {
    digitalWrite(WATER_PUMP_PIN, LOW); // Activate water pump
    Blynk.virtualWrite(VPIN_WATER_PUMP_STATUS, 1);
    pumpActive = true;
    Serial.println("Safety watering activated due to critically low soil moisture level.");
    sendSystemStatus("010");
    timer.setTimeout(safetyWaterDuration, stopWaterPump); // Schedule pump stop after safety duration
  }
}

void controlActuators() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    sendSystemStatus("001"); // Send error code as a string
    return;
  }
  int currentHour = timeinfo.tm_hour;
  controlHeatMat(globalSoilTemp);
  controlWaterPump(globalSoilMoist, globalTankLevel);
  controlLedGrowLight(globalLightLevel, currentHour);
  checkForDailyUpdate();
}
//////////////////////////////////////// Schedule Watering //////////////////////////////////////////
void wateringTask(float moist, float tankLevel){
  float mid = (lowerSoilMoistThreshold + upperSoilMoistThreshold)/2;

  Serial.println("Daily watering check started");
  if (moist < mid) {
    if (tankLevel > 10 && !pumpActive) {
      Serial.println("Soil moisture below midpoint, activating watering.");
      digitalWrite(WATER_PUMP_PIN, LOW); // Activate water pump
      Blynk.virtualWrite(VPIN_WATER_PUMP_STATUS, 1);
      pumpActive = true;
      timer.setTimeout(waterDuration, stopWaterPump); // Schedule to stop the pump
    } else {
      Serial.println("Not enough water in the tank");
      sendSystemStatus("009"); 
      Blynk.virtualWrite(VPIN_WATER_PUMP_STATUS, 0);
    }
  } else {
    Serial.println("Soil moisture above midpoint, no watering needed.");
  }
  Blynk.virtualWrite(VPIN_CURRENT_WATER_DURATION, waterDuration);
}

void checkScheduledWatering() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    sendSystemStatus("001"); // Send error code as a string
    return;
  }
  if (timeinfo.tm_hour == scheduledHour && timeinfo.tm_min == scheduledMinute) {
    wateringTask(globalSoilMoist, globalTankLevel);
  }
}

//////////////////////////////////////////  Connection ////////////////////////////////////////////////////////////////
void checkWiFiConnection() {
  if (!WiFi.isConnected()) {
    Serial.println("Reconnecting to WiFi...");
    sendSystemStatus("004");
    WiFi.reconnect();
  } else {
    sendSystemStatus("000");
  }
}

void checkBlynkConnection() {
  if (!Blynk.connected()) {
    Serial.println("Attempting to reconnect to Blynk...");
    sendSystemStatus("005");
    Blynk.connect();
  } else {
    sendSystemStatus("000");
  }
}
////////////////////////////////////////// Set up & loop /////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  // Initialize sensors
  dht.begin();
  Wire.begin();
  lightMeter.begin();
  sensors.begin();

  lastAdjustmentDate = 0;
  //Initialize GPIO pins
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  pinMode(WATER_PUMP_PIN, OUTPUT);
  pinMode(HEAT_MAT_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Ensure actuators are off initially
  digitalWrite(WATER_PUMP_PIN, HIGH);
  digitalWrite(HEAT_MAT_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Connect to WiFi network
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  
  Blynk.config(auth);
  while (!Blynk.connect()) {
    // Attempt to connect to Blynk server, but don't block indefinitely
    Serial.print(".");
    delay(500);
  }
  // Set up RTC
  setupRTC();
  fetchSunriseSunset();
  sendSystemStatus("000");

  timer.setInterval(5000L, updateSensorReadings);     // Update sensor data every 5 seconds
  timer.setInterval(60000L, controlActuators);        // Manage actuators every 60 seconds
  timer.setInterval(60000L, checkScheduledWatering);  // Compare current time with scheduled watering time every 60 seconds
  timer.setInterval(100000L, checkWiFiConnection);    // Check WiFi connection every 100 seconds
  timer.setInterval(150000L, checkBlynkConnection);   // Check Blynk connection every 150 seconds
}

void loop() {
  Blynk.run();
  timer.run();
}

/*

///////////////////////////// Error Code Meaning ///////////////////////////////////////
000 = All good 
001 = Failed to obtain time for set up RTC
002 = Fail to obtain sunrise and sunset times
003 = Fail to deserialize JSON
004 = WiFi connection error
005 = Blynk connection error
006 = Ultrasonic distance sensor error 
007 = Soil temperature sensor error
008 = Soil Moisture sensor error
009 = Not enough water in the water tank
010 = Safety watering activated due to critically low soil moisture level

*/




