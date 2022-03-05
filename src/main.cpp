#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>


#if !( defined(ESP32) )
  #error This code is intended to run on ESP32 platform! Please check your Tools->Board setting.
#endif

#include <FastLED.h>
#include <vector>

#include "wifi/WifiManager.h"
#include "modes/HoneyOrangeMode.h"
#include "modes/PaletteMode.h"

CRGB leds[LED_STRIP_SIZE];

std::vector<CBaseMode*> modes;
CWifiManager *wifiManager;

void setup() {
  delay( 1000 ); // power-up safety delay

  Serial.begin(115200);
  while(!Serial && !Serial.available()){}
  randomSeed(analogRead(0));

  Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  Log.noticeln("******************************************");  

  FastLED.addLeds<LED_TYPE, LED_PIN, LED_COLOR_ORDER>(leds, LED_STRIP_SIZE).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(255);

#ifdef LED_PIN_BOARD
  pinMode(LED_PIN_BOARD, OUTPUT);
  digitalWrite(LED_PIN_BOARD, HIGH);
#endif

  EEPROM_loadConfig();

  wifiManager = new CWifiManager();

  modes.push_back(new CPaletteMode(LED_STRIP_SIZE, "Party Colors", PartyColors_p, 255.0 / (float)LED_STRIP_SIZE));
  //modes.push_back(new CPaletteMode(LED_STRIP_SIZE, "Heat Colors", HeatColors_p, 255.0 / (float)LED_STRIP_SIZE));
  modes.push_back(new CPaletteMode(LED_STRIP_SIZE, "Rainbow Colors", RainbowColors_p, 255.0 / (float)LED_STRIP_SIZE));
  modes.push_back(new CPaletteMode(LED_STRIP_SIZE, "Cloud Colors", CloudColors_p, 255.0 / (float)LED_STRIP_SIZE));
  //modes.push_back(new CPaletteMode(LED_STRIP_SIZE, "Forest Colors", ForestColors_p, 255.0 / (float)LED_STRIP_SIZE));
  //modes.push_back(new CPaletteMode(LED_STRIP_SIZE, "Ocean Colors", OceanColors_p, 255.0 / (float)LED_STRIP_SIZE));
  //modes.push_back(new CPaletteMode(LED_STRIP_SIZE, "Lava Colors", LavaColors_p, 255.0 / (float)LED_STRIP_SIZE));
  modes.push_back(new CHoneyOrangeMode(LED_STRIP_SIZE, "Honey Amber"));
  
  wifiManager->setModes(&modes);

  Log.noticeln("Setup completed!");
}

void loop() {
  static unsigned long tsMillis = millis();
  
  wifiManager->loop();

  //Log.infoln("mode: %i, bright: %i, delay: %i, cycle: %i", configuration.ledMode, 10000.0 * configuration.ledBrightness, configuration.ledDelayMs, configuration.ledCycleModeMs);
  if (configuration.ledMode > modes.size()-1) {
    configuration.ledMode = 0;
  }

  memset(leds, 0, sizeof(CRGB)*LED_STRIP_SIZE);
  modes[configuration.ledMode]->draw(leds);
  FastLED.show(255 * configuration.ledBrightness);

  if (configuration.ledCycleModeMs > 0) {
    // Change modes every so often 
    if (millis() - tsMillis > configuration.ledCycleModeMs) {
      tsMillis = millis();
      configuration.ledMode++; 
      if (configuration.ledMode > modes.size()-1) {
        configuration.ledMode = 0;
      }
    }
  }
}
