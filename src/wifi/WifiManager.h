#ifndef _WIFI_MANAGER_H
#define _WIFI_MANAGER_H

#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
#elif ESP8266
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

#include "BaseManager.h"
#include "modes/BaseMode.h"

typedef enum {
  WF_CONNECTING = 0,
  WF_LISTENING = 1
} wifi_status;

class CWifiManager: public CBaseManager {

private:
  unsigned long tMillis;
  wifi_status status;
  char softAP_SSID[32];
  char SSID[32];
  bool apMode;

  AsyncWebServer* server;

  void connect();
  void listen();

  void handleRoot(AsyncWebServerRequest *request);
  void handleConnect(AsyncWebServerRequest *request);
  void handleLedMode(AsyncWebServerRequest *request);

  std::vector<CBaseMode*> *modes;
        
public:
	CWifiManager();
  virtual void loop();

  void setModes(std::vector<CBaseMode*> *modes) { this->modes = modes; }
};

#endif