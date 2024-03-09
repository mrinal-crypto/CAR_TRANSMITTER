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
#define GPS_POWER 35

CRGB leds[NUM_LEDS];
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


unsigned long previousMillis = 0;
const unsigned long interval = 50;


volatile uint8_t sharedVarForSpeed;
volatile uint16_t sharedVarForTime = 0;

int signalQuality[] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
                       99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 97, 97, 96, 96, 95, 95, 94, 93, 93, 92,
                       91, 90, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 76, 75, 74, 73, 71, 70, 69, 67, 66, 64,
                       63, 61, 60, 58, 56, 55, 53, 51, 50, 48, 46, 44, 42, 40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 20,
                       17, 15, 13, 10, 8, 6, 3, 1, 1, 1, 1, 1, 1, 1, 1
                      };

const int portalOpenTime = 300000; //server open for 5 mins
bool onDemand;
String firebaseStatus = "";
String ssid = "";
float batteryLevel = 11.2;
float blc = 11.1;
float bhc = 12.5;
float latti = 00.00000;
float longi = 00.00000;
float carSpeed = 00.00;
uint8_t wifiRSSI = 0;
uint8_t throttleValue;
uint8_t potValue;
uint8_t forwardValue;
uint8_t backwardValue;
uint8_t leftValue;
uint8_t rightValue;
uint8_t hornValue;
uint8_t headlightValue;
uint8_t gpsValue;

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
  pinMode(GPS_POWER, INPUT);
  delay(500);

  if (SPIFFS.begin(true)) {
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 10, "FILESYSTEM = OK!");
    u8g2.sendBuffer();
  }
  if (!SPIFFS.begin(true)) {
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 10, "FILESYSTEM = ERROR!");
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
  delay(1000);
  connectFirebase();
  delay(2000);
  u8g2.clearDisplay();
  drawLayout();
  delay(1000);

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
  u8g2.drawStr(7, 30, "ESP CAR");
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(29, 43, "TRANSMITTER");
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

    clearLCD(0, 40, 128, 10);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 50, "SECRETS EXIST...");
    u8g2.sendBuffer();
    delay(500);

    Firebase.begin(preferences.getString("firebaseUrl", ""), preferences.getString("firebaseToken", ""));
    delay(100);
    Firebase.reconnectWiFi(true);
    delay(100);

    if (isFirebaseConnected() == true) {
      Serial.println("Connected to Firebase. Skipping server setup.");
      clearLCD(0, 50, 128, 10);
      u8g2.setFont(u8g2_font_t0_11_tr);
      u8g2.drawStr(0, 60, "SERVER = OK!");
      u8g2.sendBuffer();
      delay(500);
      firebaseStatus = "ok";
    } else {
      Serial.println("Failed to connect to Firebase. Starting server setup.");
      clearLCD(0, 50, 128, 10);
      u8g2.setFont(u8g2_font_t0_11_tr);
      u8g2.drawStr(0, 60, "SERVER = ERROR!");
      u8g2.sendBuffer();
      delay(1000);
      setupServer();
    }
  } else {
    Serial.println("Firebase settings not found. Starting server setup.");
    clearLCD(0, 40, 128, 10);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(0, 50, "SECRETS NOT FOUND!");
    u8g2.sendBuffer();
    delay(500);
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
      Serial.println("Success");
      Serial.println("Restarting your device...");

      clearLCD(0, 40, 128, 10);
      u8g2.setFont(u8g2_font_t0_11_tr);
      u8g2.drawStr(0, 50, "SAVED. SUCCESS!");
      delay(500);
      clearLCD(0, 50, 128, 10);
      u8g2.drawStr(0, 60, "RESTARTING...");
      u8g2.sendBuffer();
      delay(1000);
      ESP.restart();
    } else {
      firebaseStatus = "";
      Serial.println("Firebase settings saved");
      Serial.println("Error! Check your credentials.");
      Serial.println("Restarting your device...");


      clearLCD(0, 40, 128, 10);
      u8g2.setFont(u8g2_font_t0_11_tr);
      u8g2.drawStr(0, 50, "SAVED. FAILED!");
      delay(500);
      clearLCD(0, 50, 128, 10);
      u8g2.drawStr(0, 60, "RESTARTING...");
      u8g2.sendBuffer();
      delay(1000);
      ESP.restart();
    }
  });

  server.serveStatic("/", SPIFFS, "/");
  server.begin();

  Serial.println("server begin");
  Serial.println(WiFi.localIP());

  clearLCD(0, 40, 128, 10);
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(0, 50, "SERVER OPEN");
  clearLCD(0, 50, 128, 10);
  ipCheck(0, 60);
  u8g2.sendBuffer();
  delay(500);

  showLedStatus(0, 0, 255);


  delay(portalOpenTime);
  Serial.println("Restarting your device...");

  clearLCD(0, 50, 128, 10);
  u8g2.drawStr(0, 60, "RESTARTING...");
  u8g2.sendBuffer();
  delay(1000);

  ESP.restart();
}
//////////////////////////////////////////////////////////////////////////////
void ipCheck(uint8_t ipx, uint8_t ipy) {

  String rawIP = WiFi.localIP().toString(); //toString () used for convert char to string

  String IPAdd = "IP " + rawIP;

  clearLCD(ipx, ipy - 10, 98, 10);

  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(ipx, ipy, IPAdd.c_str()); //c_str() function used for convert string to const char *
  u8g2.sendBuffer();

}
//////////////////////////////////////////////////////////////////////////////
void connectWiFi() {

  WiFiManager wm;

  clearLCD(0, 10, 128, 30);
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(0, 20, "CONNECTING WIFI...");
  u8g2.drawStr(0, 30, "AP - TRANSMITTER");
  u8g2.drawStr(0, 40, "IP - 192.168.4.1");
  u8g2.sendBuffer();

  WiFi.disconnect();
  delay(50);
  bool success = false;
  while (!success) {
    //    Serial.println("AP - espSmartHome  Setup IP - 192.168.4.1");
    wm.setConfigPortalTimeout(60);
    success = wm.autoConnect("TRANSMITTER");
    if (!success) {
      clearLCD(0, 10, 128, 30);
      u8g2.setFont(u8g2_font_t0_11_tr);
      u8g2.drawStr(0, 20, "WIFI SETUP = ERROR!");
      u8g2.drawStr(0, 30, "AP - TRANSMITTER");
      u8g2.drawStr(0, 40, "IP - 192.168.4.1");
      u8g2.sendBuffer();

      Serial.println("TRANSMITTER");
      Serial.println("Setup IP - 192.168.4.1");
      Serial.println("Conection Failed!");
    }
  }

  Serial.print("Connected SSID - ");
  Serial.println(WiFi.SSID());
  Serial.print("IP Address is : ");
  Serial.println(WiFi.localIP());

  clearLCD(0, 10, 128, 30);
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(0, 20, "WIFI SETUP = OK!");
  u8g2.sendBuffer();
  delay(1000);
  ssid = WiFi.SSID();
  u8g2.drawStr(0, 30, ssid.c_str());
  u8g2.sendBuffer();
  delay(1000);
  wifiSignalQuality(100, 30);
  delay(500);
  ipCheck(0, 40);
  delay(500);
}
////////////////////////////////////////////////////////////////////////
void wifiSignalQuality(uint8_t sqx, uint8_t sqy) {

  wifiRSSI = WiFi.RSSI() * (-1);
  char str[3];
  char str2[3] = "%";

  tostring(str, signalQuality[wifiRSSI]);

  strcat(str, str2);

  clearLCD(sqx, sqy - 9, 20, 9);

  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(sqx, sqy, str);
  u8g2.sendBuffer();
}
///////////////////////////////////////////////////////////////////////
void tostring(char str[], int num) {
  int i, rem, len = 0, n;

  n = num;
  while (n != 0)
  {
    len++;
    n /= 10;
  }
  for (i = 0; i < len; i++)
  {
    rem = num % 10;
    num = num / 10;
    str[len - (i + 1)] = rem + '0';
  }
  str[len] = '\0';
}
////////////////////////////////////////////////////////////////////////
void onDemandFirebaseConfig() {
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    u8g2.clearDisplay();
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


  StaticJsonDocument<385> doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    //    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  backwardValue = doc["BACKWARD"];
  batteryLevel = doc["BATTERY"];
  bhc = doc["BHC"];
  blc = doc["BLC"];
  forwardValue = doc["FORWARD"];
  gpsValue = doc["GPS"];
  headlightValue = doc["HL"];
  hornValue = doc["HORN"];
  latti = doc["LAT"];
  leftValue = doc["LEFT"];
  longi = doc["LNG"];
  rightValue = doc["RIGHT"];
  carSpeed = doc["SPEED"];
  throttleValue = doc["THROTTLE"];




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

//////////////////////////////////////////////////////////////
void gpsPowerControll(){
  if(digitalRead(GPS_POWER) == HIGH){
    Firebase.setInt(firebaseData, "/ESP-CAR/GPS", 1);
  }else{
    Firebase.setInt(firebaseData, "/ESP-CAR/GPS", 0);
    Firebase.setFloat(firebaseData, "/ESP-CAR/LAT", 00.0000);
    Firebase.setFloat(firebaseData, "/ESP-CAR/LNG", 00.0000);
    Firebase.setFloat(firebaseData, "/ESP-CAR/SPEED", 00.000);
  }
}
/////////////////////////////////////////////////////////////
void navigation() {

  if ((digitalRead(FORWARD) == HIGH) ^
      (digitalRead(BACKWARD) == HIGH) ^
      (digitalRead(LEFT) == HIGH) ^
      (digitalRead(RIGHT) == HIGH)) {

    if (digitalRead(FORWARD) == HIGH) {
      Firebase.setInt(firebaseData, "/ESP-CAR/FORWARD", 1);
    } else if (digitalRead(BACKWARD) == HIGH) {
      Firebase.setInt(firebaseData, "/ESP-CAR/BACKWARD", 1);
    } else if (digitalRead(LEFT) == HIGH) {
      Firebase.setInt(firebaseData, "/ESP-CAR/LEFT", 1);
    } else if (digitalRead(RIGHT) == HIGH) {
      Firebase.setInt(firebaseData, "/ESP-CAR/RIGHT", 1);
    }
  } else {
    Firebase.setInt(firebaseData, "/ESP-CAR/BACKWARD", 0);
    Firebase.setInt(firebaseData, "/ESP-CAR/FORWARD", 0);
    Firebase.setInt(firebaseData, "/ESP-CAR/LEFT", 0);
    Firebase.setInt(firebaseData, "/ESP-CAR/RIGHT", 0);
  }

  if (digitalRead(HORN) == HIGH) {
    Firebase.setInt(firebaseData, "/ESP-CAR/HORN", 1);
  } else {
    Firebase.setInt(firebaseData, "/ESP-CAR/HORN", 0);
  }

  //  uint8_t f = digitalRead(FORWARD);
  //  uint8_t b = digitalRead(BACKWARD);
  //  uint8_t l = digitalRead(LEFT);
  //  uint8_t r = digitalRead(RIGHT);
  //  uint8_t h = digitalRead(HORN);
  //
  //  if (f == HIGH && b == LOW && l == LOW && r == LOW) {
  //    Firebase.setInt(firebaseData, "/ESP-CAR/FORWARD", 1);
  //  } else {
  //    Firebase.setInt(firebaseData, "/ESP-CAR/FORWARD", 0);
  //  }
  //
  //  if (b == HIGH && f == LOW && l == LOW && r == LOW) {
  //    Firebase.setInt(firebaseData, "/ESP-CAR/BACKWARD", 1);
  //  } else {
  //    Firebase.setInt(firebaseData, "/ESP-CAR/BACKWARD", 0);
  //  }
  //
  //  if (l == HIGH && f == LOW && b == LOW && r == LOW) {
  //    Firebase.setInt(firebaseData, "/ESP-CAR/LEFT", 1);
  //  } else {
  //    Firebase.setInt(firebaseData, "/ESP-CAR/LEFT", 0);
  //  }
  //
  //  if (r == HIGH && f == LOW && b == LOW && l == LOW) {
  //    Firebase.setInt(firebaseData, "/ESP-CAR/RIGHT", 1);
  //  } else {
  //    Firebase.setInt(firebaseData, "/ESP-CAR/RIGHT", 0);
  //  }
  //
  //  if (digitalRead(HORN) == HIGH) {
  //    Firebase.setInt(firebaseData, "/ESP-CAR/HORN", 1);
  //  }
  //  else {
  //    Firebase.setInt(firebaseData, "/ESP-CAR/HORN", 0);
  //  }
}
///////////////////////////////////////////////////////////////
void speedControll() {
  //  if (xSemaphoreTake(variableMutex, portMAX_DELAY)) {
  //    sharedVarForSpeed = map(analogRead(THROTTLE), 0, 4095, 0, 255);
  //    xSemaphoreGive(variableMutex);
  //  }

  potValue = map(analogRead(THROTTLE), 0, 4095, 0, 255);
  if (abs(throttleValue - potValue) > 2) {
    Firebase.setInt(firebaseData, "/ESP-CAR/THROTTLE", potValue);
  }
}
/////////////////////////////////////////////////////////////
void speedUpload() {
  if (xSemaphoreTake(variableMutex, portMAX_DELAY)) {
    throttleValue = sharedVarForSpeed;
    xSemaphoreGive(variableMutex);
    Firebase.setInt(firebaseData, "/ESP-CAR/THROTTLE", throttleValue);
  }
}
///////////////////////////////////////////////////////////////
void drawLayout() {
  u8g2.drawFrame(0, 0, 80, 64);
  u8g2.drawFrame(81, 0, 47, 64);
  u8g2.drawLine(82, 24, 126, 24);
  u8g2.drawLine(93, 44, 115, 44);
  u8g2.drawLine(104, 38, 104, 50);
  u8g2.sendBuffer();
}
///////////////////////////////////////////////////////////////
void printSSID(uint8_t ssidx, uint8_t ssidy) {
  if (strlen(ssid.c_str()) > 6) {
    String shortSSID = ssid.substring(0, 7);
    String wifiName = shortSSID + "..";
    clearLCD(ssidx, ssidy - 9, 54, 9);

    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(ssidx, ssidy, wifiName.c_str()); //c_str() function used for convert string to const char *
    u8g2.sendBuffer();
  } else {
    String wifiName = ssid;
    clearLCD(ssidx, ssidy - 9, 54, 9);

    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(ssidx, ssidy, wifiName.c_str()); //c_str() function used for convert string to const char *
    u8g2.sendBuffer();
  }
}
///////////////////////////////////////////////////////////////
void batteryVoltage(uint8_t bvx, uint8_t bvy) {
  String level = String(batteryLevel, 2);
  String inUnit = "B=" + level + "V";
  clearLCD(bvx, bvy - 9, 54, 9);
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(bvx, bvy, inUnit.c_str()); //c_str() function used for convert string to const char *
  u8g2.sendBuffer();
}
///////////////////////////////////////////////////////////////
void batteryPercent(uint8_t bpx, uint8_t bpy) {
  float batteryFactor = 99 / (bhc - blc);
  int bat = (batteryLevel - blc) * batteryFactor;
  String percentStr = String(bat);
  String percent = percentStr + "%";
  clearLCD(bpx, bpy - 9, 20, 9);
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(bpx, bpy, percent.c_str());
  u8g2.sendBuffer();
}
///////////////////////////////////////////////////////////////
void displayThrottle(uint8_t tvx, uint8_t tvy) {
  uint8_t pot = map(analogRead(THROTTLE), 0, 4095, 0, 100);
  String potStr = String(pot);
  String potPercent = "TH=" + potStr + "%";
  clearLCD(tvx, tvy - 9, 42, 9);
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(tvx, tvy, potPercent.c_str());
  u8g2.sendBuffer();
}
///////////////////////////////////////////////////////////////
void displayHorn(uint8_t dhx, uint8_t dhy) {
  if (hornValue == 1) {
    clearLCD(dhx, dhy - 9, 12, 9);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(dhx, dhy, "HR");
    u8g2.sendBuffer();
  } else {
    clearLCD(dhx, dhy - 9, 12, 9);
  }
}
///////////////////////////////////////////////////////////////
void displayHeadlight(uint8_t dhdx, uint8_t dhdy) {
  if (headlightValue == 1) {
    clearLCD(dhdx, dhdy - 9, 12, 9);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(dhdx, dhdy, "HL");
    u8g2.sendBuffer();
  } else {
    clearLCD(dhdx, dhdy - 9, 12, 9);
  }
}
///////////////////////////////////////////////////////////////
void displayNav(uint8_t dnx, uint8_t dny) {
  if (forwardValue == 1) {
    clearLCD(dnx, dny - 9, 6, 9);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(dnx, dny, "F");
    u8g2.sendBuffer();
  } else {
    clearLCD(dnx, dny - 9, 6, 9);
  }

  if (leftValue == 1) {
    clearLCD(dnx - 17, dny + 13 - 9, 6, 9);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(dnx - 17, dny + 13, "L");
    u8g2.sendBuffer();
  } else {
    clearLCD(dnx - 17, dny + 13 - 9, 6, 9);
  }

  if (rightValue == 1) {
    clearLCD(dnx + 16, dny + 13 - 9, 6, 9);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(dnx + 16, dny + 13, "R");
    u8g2.sendBuffer();
  } else {
    clearLCD(dnx + 16, dny + 13 - 9, 6, 9);
  }

  if (backwardValue == 1) {
    clearLCD(dnx, dny + 25 - 9, 6, 9);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(dnx, dny + 25, "B");
    u8g2.sendBuffer();
  } else {
    clearLCD(dnx, dny + 25 - 9, 6, 9);
  }
}
///////////////////////////////////////////////////////////////
void displayGPSStatus(uint8_t dgx, uint8_t dgy) {
  if (gpsValue == 1) {
    clearLCD(dgx, dgy - 9, 50, 9);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(dgx, dgy, "GPS=ON");
    u8g2.sendBuffer();
  } else {
    clearLCD(dgx, dgy - 9, 50, 9);
    u8g2.setFont(u8g2_font_t0_11_tr);
    u8g2.drawStr(dgx, dgy, "GPS=OFF");
    u8g2.sendBuffer();
  }
}
///////////////////////////////////////////////////////////////
void displayLatLng(uint8_t dllx, uint8_t dlly) {

  String latStr = String(latti, 4);
  String lngStr = String(longi, 4);
  String latStr2 = "LAT=" + latStr;
  String lngStr2 = "LNG=" + lngStr;

  clearLCD(dllx, dlly - 9, 77, 9);
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(dllx, dlly, latStr2.c_str());

  clearLCD(dllx, dlly + 10 - 9, 77, 9);
  u8g2.drawStr(dllx, dlly + 10, lngStr2.c_str());
  u8g2.sendBuffer();
}
///////////////////////////////////////////////////////////////
void displayCarSpeed(uint8_t dcsx, uint8_t dcsy) {

  String speedStr = String(carSpeed, 3);
  String speedStr2 = "KMPH=" + speedStr;

  clearLCD(dcsx, dcsy - 9, 77, 9);
  u8g2.setFont(u8g2_font_t0_11_tr);
  u8g2.drawStr(dcsx, dcsy, speedStr2.c_str());
  u8g2.sendBuffer();
}

///////////////////////////////////////////////////////////////
void loop1(void * parameter) {

  for (;;) {
    drawLayout();
    if (WiFi.status() == WL_CONNECTED && firebaseStatus == "ok") {
      showLedStatus(0, 255, 0);
      printSSID(2, 10);
      wifiSignalQuality(55, 10);
      batteryVoltage(2, 20);
      batteryPercent(55, 20);
      displayThrottle(83, 11);
      displayHorn(86, 22);
      displayHeadlight(112, 22);
      displayNav(102, 36);
      displayGPSStatus(2, 30);
      displayLatLng(2, 40);
      displayCarSpeed(2, 60);

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
    gpsPowerControll();
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
