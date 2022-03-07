#if !( defined(ESP32) )
  #error This code is intended to run on ESP8266 platform! Please check your Tools->Board setting.
#endif

#include <Arduino.h>
#include <WiFiClient.h>
#include <ezTime.h>
#include <AsyncElegantOTA.h>

#include "wifi/WifiManager.h"
#include "Configuration.h"

#define MAX_CONNECT_TIMEOUT_MS 15000 // 10 seconds to connect before creating its own AP
#define BOARD_LED_PIN 2

#ifdef ESP32
  #define ESP_LABEL "ESP32"
#elif ESP8266
  #define ESP_LABEL "ESP8266"
#endif

const int RSSI_MAX =-50;// define maximum straighten of signal in dBm
const int RSSI_MIN =-100;// define minimum strength of signal in dBm

WiFiClient espClient;

int dBmtoPercentage(int dBm) {
  int quality;
  if(dBm <= RSSI_MIN) {
    quality = 0;
  } else if(dBm >= RSSI_MAX) {  
    quality = 100;
  } else {
    quality = 2 * (dBm + 100);
  }
  return quality;
}

const String htmlTop FL_PROGMEM = "<html>\
  <head>\
    <title>%s LED Leaf</title>\
    <style>\
      body { background-color: #303030; font-family: 'Anaheim',sans-serif; Color: #d8d8d8; }\
    </style>\
  </head>\
  <body>\
    <h1>%s LED Leaf</h1>";

const String htmlBottom FL_PROGMEM = "<br><br><hr>\
  <p>Uptime: %02d:%02d:%02d</p>\
  </body>\
</html>";

const String htmlWifiApConnectForm FL_PROGMEM = "<h2>Connect to WiFi Access Point (AP)</h2>\
    <form method='POST' action='/connect' enctype='application/x-www-form-urlencoded'>\
      <label for='ssid'>SSID (AP Name):</label><br>\
      <input type='text' id='ssid' name='ssid'><br><br>\
      <label for='pass'>Password (WPA2):</label><br>\
      <input type='password' id='pass' name='password' minlength='8' autocomplete='off' required><br><br>\
      <input type='submit' value='Connect...'>\
    </form>";

const String htmlLEDModes FL_PROGMEM = "<hr><h2>LED Mode Selector</h2>\
    <form method='POST' action='/led_mode' enctype='application/x-www-form-urlencoded'>\
      <label for='led_mode'>LED Mode:</label><br>\
      <select name='led_mode' id='led_mode'>\
      %s\
      </select><br>\
      <br>\
      <label for='brightness'>Brightness:</label><br>\
      <input type='text' id='brightness' name='brightness' value='%.2f'> range 0.0-1.0<br>\
      <br>\
      <label for='frame_delay'>Frame delay:</label><br>\
      <input type='text' id='frame_delay' name='frame_delay' value='%i'> milliseconds<br>\
      <br>\
      <label for='cycle_delay'>Auto cycle modes every:</label><br>\
      <input type='text' id='cycle_delay' name='cycle_delay' value='%i'> seconds (0-stay on current mode)<br>\
      <br>\
      <input type='submit' value='Set...'>\
    </form>";

CWifiManager::CWifiManager(): 
apMode(false) {    
  pinMode(BOARD_LED_PIN,OUTPUT);
  strcpy(SSID, configuration.wifiSsid);
  server = new AsyncWebServer(WEB_SERVER_PORT);
  connect();
}

void CWifiManager::connect() {

  status = WF_CONNECTING;
  strcpy(softAP_SSID, "");
  tMillis = millis();

  if (strlen(SSID)) {

    // Join AP from Config
    Log.infoln("Connecting to WiFi: '%s'", SSID);
    WiFi.begin(SSID, configuration.wifiPassword);
    apMode = false;
    
  } else {

    // Create AP using fallback and chip ID
    uint32_t chipId = 0;
    #ifdef ESP32
      for(int i=0; i<17; i=i+8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
      }
    #elif ESP8266
      chipId = ESP.getChipId();
    #endif
  
    Log.infoln("Chip ID: '%i'", chipId);
    sprintf_P(softAP_SSID, "%s_%i", WIFI_FALLBACK_SSID, chipId);
    Log.infoln("Creating WiFi: '%s' / '%s'", softAP_SSID, WIFI_FALLBACK_PASS);
    
    if (WiFi.softAP(softAP_SSID, WIFI_FALLBACK_PASS)) {
      apMode = true;
      Log.infoln("Wifi AP '%s' created, listening on '%s'", softAP_SSID, WiFi.softAPIP().toString().c_str());
    } else {
      Log.errorln("Wifi AP faliled");
    };

  }
  
}

void CWifiManager::listen() {

  status = WF_LISTENING;

  // Web
  server->on("/", std::bind(&CWifiManager::handleRoot, this, std::placeholders::_1));
  server->on("/connect", HTTP_POST, std::bind(&CWifiManager::handleConnect, this, std::placeholders::_1));
  server->on("/led_mode", HTTP_POST, std::bind(&CWifiManager::handleLedMode, this, std::placeholders::_1));
  server->begin();
  Log.infoln("Web server listening on %s port %i", WiFi.localIP().toString().c_str(), WEB_SERVER_PORT);

  // NTP
  Log.infoln("Configuring time from %s at %i (%i)", configuration.ntpServer, configuration.gmtOffset_sec, configuration.daylightOffset_sec);

  // OTA
  AsyncElegantOTA.begin(server);

  configTime(configuration.gmtOffset_sec, configuration.daylightOffset_sec, configuration.ntpServer);
  struct tm timeinfo;
  
  /*
  //time()
  if(getLocalTime(&timeinfo)){
    Log.infoln("%i:%i", timeinfo.tm_hour,timeinfo.tm_min);
  }
  */
  
}

void CWifiManager::loop() {

  if (WiFi.status() == WL_CONNECTED || apMode ) {
    // WiFi is connected

    if (status != WF_LISTENING) {  
      // Start listening for requests
      listen();
    }

  } else {
    // WiFi is down

    switch (status) {
      case WF_LISTENING: {
        Log.infoln("Disconnecting %i", status);
        server->end();
        status = WF_CONNECTING;
        connect();
      } break;
      case WF_CONNECTING: {
        if (millis() - tMillis > MAX_CONNECT_TIMEOUT_MS) {
          Log.warning("Connecting failed (wifi status %i) after %l ms, create an AP instead", (millis() - tMillis), WiFi.status());
          tMillis = millis();
          strcpy(SSID, "");
          connect();
        }
      } break;

    }

  }
  
}

void CWifiManager::handleRoot(AsyncWebServerRequest *request) {

  Log.infoln("handleRoot");
  
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->printf(htmlTop.c_str(), ESP_LABEL);

  if (apMode) {
    response->printf(htmlWifiApConnectForm.c_str());
  } else {
    response->printf("<p>Connected to '%s'</p>", SSID);
  }
  
  String modeOptions = "";
  if (modes != NULL) {
    for(uint8_t i=0; i<modes->size(); i++) {
      modeOptions += String("<option") + String(i == configuration.ledMode ? " selected" : "") + String(" value='") + String(i) + String("'>") + (*modes)[i]->getName() + String("</option>");
    }
  }
  
  response->printf(htmlLEDModes.c_str(), modeOptions.c_str(), configuration.ledBrightness, configuration.ledDelayMs, configuration.ledCycleModeMs / 1000);

  response->printf(htmlBottom.c_str(), hr, min % 60, sec % 60);
  request->send(response);
}

void CWifiManager::handleConnect(AsyncWebServerRequest *request) {

  Log.info("handleConnect");

  String ssid = request->arg("ssid");
  String password = request->arg("password");
  
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->printf(htmlTop.c_str(), ESP_LABEL);
  response->printf("<p>Connecting to '%s' ... see you on the other side!</p>", ssid.c_str());
  response->printf(htmlBottom.c_str(), hr, min % 60, sec % 60);
  request->send(response);

  ssid.toCharArray(configuration.wifiSsid, sizeof(configuration.wifiSsid));
  password.toCharArray(configuration.wifiPassword, sizeof(configuration.wifiPassword));

  Log.noticeln("Saved config SSID: '%s'", configuration.wifiSsid);

  EEPROM_saveConfig();

  strcpy(SSID, configuration.wifiSsid);
  connect();
}

void CWifiManager::handleLedMode(AsyncWebServerRequest *request) {

  Log.info("handleLedMode");

  if (modes != NULL) {
    uint8_t ledMode = atoi(request->arg("led_mode").c_str());
    if (ledMode<modes->size()) {
      configuration.ledMode = ledMode;
    }
  }

  float ledBrightness = atof(request->arg("brightness").c_str());
  if (ledBrightness>=0.0 && ledBrightness<=1.0) {
    configuration.ledBrightness = ledBrightness;
  }

  configuration.ledDelayMs = atol(request->arg("frame_delay").c_str());
  configuration.ledCycleModeMs = atol(request->arg("cycle_delay").c_str());
    
  Log.noticeln("ledMode: '%i'", configuration.ledMode);
  Log.noticeln("ledBrightness: '%D'", configuration.ledBrightness);

  EEPROM_saveConfig();
  
  request->redirect("/");
}