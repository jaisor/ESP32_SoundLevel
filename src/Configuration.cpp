#include <Arduino.h>
#include <EEPROM.h>
#include "Configuration.h"

configuration_t configuration;

void EEPROM_saveConfig() {
  Log.infoln("Saving configuration to EEPROM");
  EEPROM.put(EEPROM_CONFIGURATION_START, configuration);
  EEPROM.commit();
}

void EEPROM_loadConfig() {

  memset(&configuration, 0, sizeof(configuration_t));
  EEPROM.begin(sizeof(configuration_t));
  EEPROM.get(EEPROM_CONFIGURATION_START, configuration);

  Log.noticeln("Configuration loaded: %s", configuration._loaded);

  if (strcmp(configuration._loaded, "jaisor")) {
    // blank
    Log.infoln("Blank configuration, loading defaults");
    strcpy(configuration._loaded, "jaisor");
    #ifdef LED
      configuration.ledBrightness = LED_BRIGHTNESS;
      strcpy(configuration.ntpServer, NTP_SERVER);
      configuration.gmtOffset_sec = NTP_GMT_OFFSET_SEC;
      configuration.daylightOffset_sec = NTP_DAYLIGHT_OFFSET_SEC;
      configuration.ledMode = 0;
      configuration.ledCycleModeMs = LED_CHANGE_MODE_SEC * 1000;
      configuration.ledDelayMs = 10;
      configuration.ledBrightness = LED_BRIGHTNESS;
    #endif
  }

#ifdef LED
  if (isnan(configuration.ledBrightness)) {
    Log.verboseln("NaN brightness");
    configuration.ledBrightness = LED_BRIGHTNESS;
  }
  if (isnan(configuration.ledMode)) {
    Log.verboseln("NaN ledMode");
    configuration.ledMode = 0;
  }
  if (isnan(configuration.ledCycleModeMs)) {
    Log.verboseln("NaN ledCycleModeMs");
    configuration.ledCycleModeMs = 0;
  }
  if (isnan(configuration.ledDelayMs)) {
    Log.verboseln("NaN ledDelayMs");
    configuration.ledDelayMs = 10;
  }
#endif

#ifdef WIFI
  String wifiStr = String(configuration.wifiSsid);
  for (auto i : wifiStr) {
    if (!isAscii(i)) {
      Log.verboseln("Bad SSID, loading default: %s", wifiStr.c_str());
      strcpy(configuration.wifiSsid, "");
      break;
    }
  }
#endif

  // FIXME: Always default NTP values
  strcpy(configuration.ntpServer, NTP_SERVER);
  configuration.gmtOffset_sec = NTP_GMT_OFFSET_SEC;
  configuration.daylightOffset_sec = NTP_DAYLIGHT_OFFSET_SEC;

}

void EEPROM_wipe() {
  Log.warningln("Wiping configuration!");
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
}