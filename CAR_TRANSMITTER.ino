#include <Arduino.h>
#include <WiFi.h> //works for only esp32
#include <WiFiManager.h> //works for only esp32
#include <FirebaseESP8266.h> //works for both esp32 and esp8266
#include <ArduinoJson.h>
#include <FastLED.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include "SPIFFS.h"
#include <FS.h>

#define DATA_PIN 12
#define NUM_LEDS 1
#define CHIPSET WS2812
#define BRIGHTNESS 50
#define COLOR_ORDER GRB
#define STATUS_LED 0
#define BOOT_BUTTON_PIN 0

CRGB leds[NUM_LEDS];



#define SWITCH1 16
#define SWITCH2 17
#define SWITCH3 18
#define SWITCH4 19


unsigned long previousMillis = 0;
const unsigned long interval = 50;

const int portalOpenTime = 300000; //server open for 5 mins
bool onDemand;
uint8_t batteryLevel;

String firebaseStatus = "";





const char* switch2;
const char* switch3;
const char* switch4;

unsigned int s1;
unsigned int s2;
unsigned int s3;
unsigned int s4;

FirebaseData firebaseData;
AsyncWebServer server(80);
Preferences preferences;
TaskHandle_t Task1;
SemaphoreHandle_t variableMutex;

void setup() {
  Serial.begin(115200);



  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }

  pinMode(BOOT_BUTTON_PIN, INPUT);
  pinMode(SWITCH1, OUTPUT);
  pinMode(SWITCH2, OUTPUT);
  pinMode(SWITCH3, OUTPUT);
  pinMode(SWITCH4, OUTPUT);
  FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();


  connectWiFi();
  connectFirebase();

  variableMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(
    loop1,
    "Task1",
    10000,
    NULL,
    1,
    &Task1,
    1);
  delay(500);
}


void connectFirebase() {
  preferences.begin("my-app", false);

  if (preferences.getString("firebaseUrl", "") != "" && preferences.getString("firebaseToken", "") != "") {
    Serial.println("Firebase settings already exist. Checking Firebase connection...");

    Firebase.begin(preferences.getString("firebaseUrl", ""), preferences.getString("firebaseToken", ""));
    delay(100);
    Firebase.reconnectWiFi(true);
    delay(100);

    if (isFirebaseConnected() == true) {
      Serial.println("Connected to Firebase. Skipping server setup.");
      firebaseStatus = "ok";
    } else {
      Serial.println("Failed to connect to Firebase. Starting server setup.");
      setupServer();
    }
  } else {
    Serial.println("Firebase settings not found. Starting server setup.");
    setupServer();
  }

}


void setupServer() {
  preferences.begin("my-app", false);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", String(), false);
  });

  server.on("/Submit", HTTP_POST, [](AsyncWebServerRequest * request) {

    String firebaseUrl = request->arg("url");
    String firebaseToken = request->arg("token");

    preferences.putString("firebaseUrl", firebaseUrl);
    preferences.putString("firebaseToken", firebaseToken);

    Firebase.begin(firebaseUrl, firebaseToken);
    delay(100);
    Firebase.reconnectWiFi(true);
    delay(100);

    if (isFirebaseConnected() == true) {
      firebaseStatus = "ok";
      Serial.println("Firebase settings saved");
      delay(300);
      Serial.println("Success");
      delay(300);
      Serial.println("Restarting your device...");
      delay(500);
      ESP.restart();
    } else {
      firebaseStatus = "";
      Serial.println("Firebase settings saved");
      delay(300);
      Serial.println("Error! Check your credentials.");
      delay(300);
      Serial.println("Restarting your device...");
      delay(500);
      ESP.restart();
    }
  });

  server.serveStatic("/", SPIFFS, "/");
  server.begin();

  Serial.println("server begin");
  Serial.println(WiFi.localIP());

  showLedStatus(0, 0, 255);


  delay(portalOpenTime);
  Serial.println("Restarting your device...");
  delay(1000);
  ESP.restart();
}

void connectWiFi() {

  WiFiManager wm;
  WiFi.disconnect();
  delay(50);
  bool success = false;
  while (!success) {
    //    Serial.println("AP - espSmartHome  Setup IP - 192.168.4.1");
    wm.setConfigPortalTimeout(60);
    success = wm.autoConnect("TRANSMITTER");
    if (!success) {
      Serial.println("TRANSMITTER");
      Serial.println("Setup IP - 192.168.4.1");
      Serial.println("Conection Failed!");
    }
  }

  Serial.print("Connected SSID - ");
  Serial.println(WiFi.SSID());
  Serial.print("IP Address is : ");
  Serial.println(WiFi.localIP());
  delay(3000);
}

void onDemandFirebaseConfig() {
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    onDemand = true;
    firebaseStatus = "";
    setupServer();
  }
  delay(100);
}

void decodeData(String data) {

  Serial.println(data); //For Example=> {"value1":"\"on\"","value2":"\"on\"","value3":"\"off\"","value4":"\"off\""}

  /*
      goto website https://arduinojson.org/v6/assistant/#/step1
      select board
      choose input datatype
      and paste your JSON data
      it automatically generate your code
  */


  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    //    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  batteryLevel = doc["BATTERY"]; // "110"
  Serial.println(batteryLevel);


}

boolean isFirebaseConnected() {
  Firebase.getString(firebaseData, "/ESP-CAR");
  if (firebaseData.stringData() != "") {
    return true;
  }
  else {
    return false;
  }
}


//////////////////////////////////////////////////////////////
void showLedStatus(uint8_t r, uint8_t g, uint8_t b ) {
  leds[STATUS_LED] = CRGB(r, g, b);;
  FastLED.show();
}

///////////////////////////////////////////////////////////////

void loading()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS - 1) - pixelnumber;

    nblend( leds[pixelnumber], newcolor, 64);
  }
}



///////////////////////////////////////////////////////////////
void loop1(void * parameter) {

  for (;;) {

    if (WiFi.status() == WL_CONNECTED && firebaseStatus == "ok") {
      showLedStatus(0, 255, 0);

    }

    if (onDemand == true) {
      loading();
      FastLED.show();
    }
    if (WiFi.status() != WL_CONNECTED) {
      showLedStatus(255, 0, 0);
      connectWiFi();
    }
  }
}


/////////////////////////////////////////////////////////////
void generateRandom() {

  int backward = random(0, 100);
  int forward = random(0, 100);
  int left = random(0, 100);
  int right = random(0, 100);
  int speed = random(0, 255);
  int horn = random(0, 100);
  int battery = random(0, 100);


  Firebase.setInt(firebaseData, "/ESP-CAR/BACKWARD", backward);
  Firebase.setInt(firebaseData, "/ESP-CAR/BATTERY", battery);
  Firebase.setInt(firebaseData, "/ESP-CAR/FORWARD", forward);
  Firebase.setInt(firebaseData, "/ESP-CAR/HORN", horn);
  Firebase.setInt(firebaseData, "/ESP-CAR/LEFT", left);
  Firebase.setInt(firebaseData, "/ESP-CAR/RIGHT", right);
  Firebase.setInt(firebaseData, "/ESP-CAR/SPEED", speed);



}


//////////////////////////////////////////////////////////////

void loop() {

  onDemand = false;
  onDemandFirebaseConfig();

  if (firebaseStatus == "ok") {

    Firebase.getString(firebaseData, "/ESP-CAR");
    decodeData(firebaseData.stringData());

    //    generateRandom();


    //    controlSwitch1(s1);
    //    controlSwitch2(s2);
    //    controlSwitch3(s3);
    //    controlSwitch4(s4);
  }
  else {
    Serial.println("firebase failed");
  }


  if (firebaseStatus != "ok") {
    if (WiFi.status() == WL_CONNECTED) {
      Firebase.getString(firebaseData, "/ESP-CAR");
      decodeData(firebaseData.stringData());
    }
  }

}
