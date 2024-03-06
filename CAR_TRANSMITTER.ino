#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <FirebaseESP8266.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include "SPIFFS.h"
#include <FS.h>

#define DATA_PIN 32
#define NUM_LEDS 1
#define CHIPSET WS2812
#define BRIGHTNESS 50
#define COLOR_ORDER GRB
#define STATUS_LED 0
#define BOOT_BUTTON_PIN 0

#define THROTTLE 34
#define FORWARD 16
#define BACKWARD 17
#define LEFT 18
#define RIGHT 19
#define HORN 4

CRGB leds[NUM_LEDS];
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


unsigned long previousMillis = 0;
const unsigned long interval = 50;

const int portalOpenTime = 300000; //server open for 5 mins
bool onDemand;


String firebaseStatus = "";
volatile uint8_t sharedVarForSpeed;
volatile uint16_t sharedVarForTime = 0;




const char* switch2;
const char* switch3;
const char* switch4;

uint8_t batteryLevel;
uint8_t throttleValue;
uint8_t forwardValue;
uint8_t backwardValue;
uint8_t leftValue;
uint8_t rightValue;
uint8_t hornValue;

FirebaseData firebaseData;
AsyncWebServer server(80);
Preferences preferences;

TaskHandle_t Task1;
SemaphoreHandle_t variableMutex;

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  delay(100);
  welcomeMsg();
  delay(3000);
  u8g2.clearDisplay();


  pinMode(BOOT_BUTTON_PIN, INPUT);
  pinMode(THROTTLE, INPUT);
  pinMode(FORWARD, INPUT);
  pinMode(BACKWARD, INPUT);
  pinMode(LEFT, INPUT);
  pinMode(RIGHT, INPUT);
  pinMode(HORN, INPUT);
  delay(500);

  if (SPIFFS.begin(true)) {
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 9, "filesystem = ok!");
    u8g2.sendBuffer();
  }
  if (!SPIFFS.begin(true)){
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 9, "filesystem = error!");
    u8g2.sendBuffer();
  }
  delay(1000);

  FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  delay(500);


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
////////////////////////////////////////////////////////////////////////
void welcomeMsg() {

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_luBIS18_tr);
  u8g2.drawStr(7, 35, "ESP CAR");
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(2, 60, "developed by M.Maity");
  u8g2.sendBuffer();
  u8g2.clearBuffer();

}
////////////////////////////////////////////////////////////////////////
void clearLCD(const long x, uint8_t y, uint8_t wid, uint8_t hig) {
  /*  this wid is right x, this height is below y
      where font wid is right x, font height is upper y
  */
  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y, wid, hig);
  u8g2.setDrawColor(1);
}
/////////////////////////////////////////////////////////////////////////
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

  batteryLevel = doc["BATTERY"];
  throttleValue = doc["SPEED"];
  //  Serial.println(batteryLevel);


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

void navigation() {
  uint8_t f = digitalRead(FORWARD);
  uint8_t b = digitalRead(BACKWARD);
  uint8_t l = digitalRead(LEFT);
  uint8_t r = digitalRead(RIGHT);
  uint8_t h = digitalRead(HORN);

  if (f == HIGH && b == LOW && l == LOW && r == LOW) {
    Firebase.setInt(firebaseData, "/ESP-CAR/FORWARD", 1);
  } else {
    Firebase.setInt(firebaseData, "/ESP-CAR/FORWARD", 0);
  }

  if (b == HIGH && f == LOW && l == LOW && r == LOW) {
    Firebase.setInt(firebaseData, "/ESP-CAR/BACKWARD", 1);
  } else {
    Firebase.setInt(firebaseData, "/ESP-CAR/BACKWARD", 0);
  }

  if (l == HIGH && f == LOW && b == LOW && r == LOW) {
    Firebase.setInt(firebaseData, "/ESP-CAR/LEFT", 1);
  } else {
    Firebase.setInt(firebaseData, "/ESP-CAR/LEFT", 0);
  }

  if (r == HIGH && f == LOW && b == LOW && l == LOW) {
    Firebase.setInt(firebaseData, "/ESP-CAR/RIGHT", 1);
  } else {
    Firebase.setInt(firebaseData, "/ESP-CAR/RIGHT", 0);
  }

  if (digitalRead(HORN) == HIGH) {
    Firebase.setInt(firebaseData, "/ESP-CAR/HORN", 1);
  }
  else {
    Firebase.setInt(firebaseData, "/ESP-CAR/HORN", 0);
  }
}
///////////////////////////////////////////////////////////////
void speedControll() {
  //  if (xSemaphoreTake(variableMutex, portMAX_DELAY)) {
  //    sharedVarForSpeed = map(analogRead(THROTTLE), 0, 4095, 0, 255);
  //    xSemaphoreGive(variableMutex);
  //  }

  int mappedValue = map(analogRead(THROTTLE), 0, 4095, 0, 255);
  //  Serial.println(mappedValue);

  if (abs(throttleValue - mappedValue) > 2) {
    Firebase.setInt(firebaseData, "/ESP-CAR/SPEED", mappedValue);
  }
}
/////////////////////////////////////////////////////////////
void speedUpload() {
  if (xSemaphoreTake(variableMutex, portMAX_DELAY)) {
    throttleValue = sharedVarForSpeed;
    xSemaphoreGive(variableMutex);
    Firebase.setInt(firebaseData, "/ESP-CAR/SPEED", throttleValue);
  }
}
//////////////////////////////////////////////////////////////
void updateFirebase() {

  Firebase.setInt(firebaseData, "/ESP-CAR/FORWARD", forwardValue);
  Firebase.setInt(firebaseData, "/ESP-CAR/BACKWARD", backwardValue);
  Firebase.setInt(firebaseData, "/ESP-CAR/LEFT", leftValue);
  Firebase.setInt(firebaseData, "/ESP-CAR/RIGHT", rightValue);
  Firebase.setInt(firebaseData, "/ESP-CAR/HORN", hornValue);
  Firebase.setInt(firebaseData, "/ESP-CAR/SPEED", throttleValue);
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

//////////////////////////////////////////////////////////////

void loop() {

  onDemand = false;
  onDemandFirebaseConfig();

  if (firebaseStatus == "ok") {
    navigation();
    speedControll();
    Firebase.getString(firebaseData, "/ESP-CAR");
    decodeData(firebaseData.stringData());
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
