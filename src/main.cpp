#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>

#if !( defined(ESP32) )
  #error This code is intended to run on ESP32 platform! Please check your Tools->Board setting.
#endif

#include "Configuration.h"
#include "Device.h"
#include "wifi/WifiManager.h"

CDevice *device;
CWifiManager *wifiManager;

unsigned long tsSmoothBoot;
bool smoothBoot;
unsigned long tsMillisBooted;


void setup() {
  Serial.begin(115200);  while (!Serial); delay(200);
  randomSeed(analogRead(0));
  
  pinMode(INTERNAL_LED_PIN, OUTPUT);
  #ifdef ESP32
    digitalWrite(INTERNAL_LED_PIN, HIGH);
  #elif ESP8266
    digitalWrite(INTERNAL_LED_PIN, LOW);
  #endif

  Log.begin(LOG_LEVEL_NOTICE, &Serial);
  Log.noticeln("Initializing...");  

  if (EEPROM_initAndCheckFactoryReset() >= 3) {
    Log.warningln("Factory reset conditions met!");
    EEPROM_wipe();  
  }

  tsSmoothBoot = millis();
  smoothBoot = false;

  EEPROM_loadConfig();

  device = new CDevice();
  wifiManager = new CWifiManager();

  Log.infoln("Initialized");
}

void loop() {

  if (!smoothBoot && millis() - tsSmoothBoot > FACTORY_RESET_CLEAR_TIMER_MS) {
    smoothBoot = true;
    EEPROM_clearFactoryReset();
    tsMillisBooted = millis();
    Log.noticeln("Device booted smoothly!");

    #ifdef ESP32
      digitalWrite(INTERNAL_LED_PIN, LOW);
    #elif ESP8266
      digitalWrite(INTERNAL_LED_PIN, HIGH);
    #endif
  }

  device->loop();
  wifiManager->loop();

  if (wifiManager->isRebootNeeded()) {
    return;
  }

#ifdef OLED
  Adafruit_SSD1306 *display = device->display();

  display->clearDisplay();

  // DEBUG: show key resistence value and bitmap
  display->setTextSize(1);
  display->setCursor(70, 24);
  display->print("K:");

  display->display();
#endif

  delay(20);
  yield();
}
