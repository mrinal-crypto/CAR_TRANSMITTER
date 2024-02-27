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


TaskHandle_t Task2;
SemaphoreHandle_t variableMutex;

void setup() {
  
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

void loop2(void * parameter) {
  for (;;) {

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void loop() {
  
}
