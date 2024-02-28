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


FirebaseData firebaseData;
AsyncWebServer server(80);
Preferences preferences;
TaskHandle_t Task2;
SemaphoreHandle_t variableMutex;

void setup() {
  Serial.begin(115200);
  



  connectWiFi();
  
  variableMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(
    loop2,
    "Task2",
    100000,
    NULL,
    1,
    &Task2,
    1);
  delay(500);
}

//////////////////////////////////////////////////////
void connectWiFi() {
  WiFiManager wm;
  WiFi.disconnect();
  delay(50);
  bool success = false;
  while (!success) {
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
//////////////////////////////////////////////////////
void loop2(void * parameter) {
  for (;;) {

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
//////////////////////////////////////////////////////
void loop() {
  
}
