#if !( defined(ESP32) ) && !( defined(ESP8266) )
  #error This code is intended to run on ESP8266 or ESP32 platform!
#endif

#include <Arduino.h>
#include <WiFiClient.h>
#include <Time.h>
#include <ezTime.h>
#include <AsyncElegantOTA.h>
#include <StreamUtils.h>

#include "wifi/WifiManager.h"
#include "Configuration.h"

#define MAX_CONNECT_TIMEOUT_MS 15000 // 10 seconds to connect before creating its own AP

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

#ifdef ESP8266
bool getLocalTime(struct tm * info, uint32_t ms=5000) {
  uint32_t start = millis();
  time_t now;
  while((millis()-start) <= ms) {
    time(&now);
    localtime_r(&now, info);
    if(info->tm_year > (2016 - 1900)){
      return true;
    }
    delay(10);
  }
  return false;
}
#endif

const String htmlTop = "<html>\
  <head>\
  <title>%s</title>\
  <style>\
    body { background-color: #303030; font-family: 'Anaheim',sans-serif; Color: #d8d8d8; }\
  </style>\
  </head>\
  <body>\
  <h1>%s - Smart Pool Thermometer</h1>%s";

const String htmlBottom = "<br><br><hr>\
  <p><b>%s</b><br>\
  Uptime: <b>%02d:%02d:%02d</b><br>\
  WiFi Signal Strength: <b>%i%%</b>\
  </p></body>\
</html>";

const String htmlWifiApConnectForm = "<hr><h2>Connect to WiFi Access Point (AP)</h2>\
  <form method='POST' action='/connect' enctype='application/x-www-form-urlencoded'>\
    <label for='ssid'>SSID (AP Name):</label><br>\
    <input type='text' id='ssid' name='ssid'><br><br>\
    <label for='pass'>Password (WPA2):</label><br>\
    <input type='password' id='pass' name='password' minlength='8' autocomplete='off' required><br><br>\
    <input type='submit' value='Connect...'>\
  </form>";

const String htmlDeviceConfigs = "<hr><h2>Configs</h2>\
  <form method='POST' action='/config' enctype='application/x-www-form-urlencoded'>\
    <label for='deviceName'>Device name:</label><br>\
    <input type='text' id='deviceName' name='deviceName' value='%s'><br>\
    <br>\
    <label for='mqttServer'>MQTT server:</label><br>\
    <input type='text' id='mqttServer' name='mqttServer' value='%s'><br>\
    <label for='mqttPort'>MQTT port:</label><br>\
    <input type='text' id='mqttPort' name='mqttPort' value='%u'><br>\
    <label for='mqttTopic'>MQTT topic:</label><br>\
    <input type='text' id='mqttTopic' name='mqttTopic' value='%s'><br>\
    <label for='mqttDataType'>Publish MQTT data as:</label><br>\
    <select name='mqttDataType' id='mqttDataType'>\
    %s\
    </select><br>\
    <br>\
    <input type='submit' value='Set...'>\
  </form>";

CWifiManager::CWifiManager(): 
rebootNeeded(false), postedUpdate(false) {  

  uptimeMillis = millis();

  postMQTTJson["name"] = configuration.name;
  
  strcpy(SSID, configuration.wifiSsid);
  server = new AsyncWebServer(WEB_SERVER_PORT);
  mqtt.setClient(espClient);
  connect();
}

void CWifiManager::connect() {

  status = WF_CONNECTING;
  strcpy(softAP_SSID, "");
  tMillis = millis();

  uint32_t deviceId = getDeviceId();
  postMQTTJson["deviceId"] = deviceId;
  Log.infoln("Device ID: '%i'", deviceId);

  if (strlen(SSID)) {

    // Join AP from Config
    Log.infoln("Connecting to WiFi: '%s'", SSID);
    WiFi.begin(SSID, configuration.wifiPassword);

  } else {

    // Create AP using fallback and chip ID
    sprintf_P(softAP_SSID, "%s_%i", WIFI_FALLBACK_SSID, deviceId);
    Log.infoln("Creating WiFi: '%s' / '%s'", softAP_SSID, WIFI_FALLBACK_PASS);

    if (WiFi.softAP(softAP_SSID, WIFI_FALLBACK_PASS)) {
      Log.infoln("Wifi AP '%s' created, listening on '%s'", softAP_SSID, WiFi.softAPIP().toString().c_str());
    } else {
      Log.errorln("Wifi AP faliled");
    };

  }
  
}

uint32_t CWifiManager::getDeviceId() {
    // Create AP using fallback and chip ID
  uint32_t chipId = 0;
  #ifdef ESP32
    for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
  #elif ESP8266
    chipId = ESP.getChipId();
  #endif

  return chipId;
}

void CWifiManager::listen() {

  status = WF_LISTENING;

  // Web
  server->on("/", std::bind(&CWifiManager::handleRoot, this, std::placeholders::_1));
  server->on("/connect", HTTP_POST, std::bind(&CWifiManager::handleConnect, this, std::placeholders::_1));
  server->on("/config", HTTP_POST, std::bind(&CWifiManager::handleConfig, this, std::placeholders::_1));
  server->on("/factory_reset", HTTP_POST, std::bind(&CWifiManager::handleFactoryReset, this, std::placeholders::_1));
  server->begin();
  Log.infoln("Web server listening on %s port %i", WiFi.localIP().toString().c_str(), WEB_SERVER_PORT);
  
  postMQTTJson["ip"] = WiFi.localIP();

  // NTP
  Log.infoln("Configuring time from %s at %i (%i)", configuration.ntpServer, configuration.gmtOffset_sec, configuration.daylightOffset_sec);
  configTime(configuration.gmtOffset_sec, configuration.daylightOffset_sec, configuration.ntpServer);
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){
    Log.noticeln("The time is %i:%i", timeinfo.tm_hour,timeinfo.tm_min);
  }

  // OTA
  AsyncElegantOTA.begin(server);

  // MQTT
  mqtt.setServer(configuration.mqttServer, configuration.mqttPort);

  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;
  mqtt.setCallback(std::bind( &CWifiManager::mqttCallback, this, _1,_2,_3));

  if (strlen(configuration.mqttServer) && strlen(configuration.mqttTopic) && !mqtt.connected()) {
    Log.noticeln("Attempting MQTT connection to '%s:%i' ...", configuration.mqttServer, configuration.mqttPort);
    if (mqtt.connect(String(getDeviceId()).c_str())) {
      Log.noticeln("MQTT connected");
      
      sprintf_P(mqttSubcribeTopicConfig, "%s/%u/config", configuration.mqttTopic, getDeviceId());
      bool r = mqtt.subscribe(mqttSubcribeTopicConfig);
      Log.noticeln("Subscribed for config changes to MQTT topic '%s' success = %T", mqttSubcribeTopicConfig, r);

      postSensorUpdate();
    } else {
      Log.warningln("MQTT connect failed, rc=%i", mqtt.state());
    }
  }
}

void CWifiManager::loop() {

  if (rebootNeeded && millis() - tMillis > 200) {
  Log.noticeln("Rebooting...");
#ifdef ESP32
  ESP.restart();
#elif ESP8266
  ESP.reset();
#endif
  return;
  }

  if (WiFi.status() == WL_CONNECTED || isApMode() ) {
  // WiFi is connected

  if (status != WF_LISTENING) {  
    // Start listening for requests
    listen();
    return;
  }

  mqtt.loop();

  if (millis() - tMillis > (postedUpdate || isApMode() ? 30000 : 1000) &&
    strlen(configuration.mqttServer) && strlen(configuration.mqttTopic) && mqtt.connected()) {
    tMillis = millis();
    postSensorUpdate();
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
      Log.warning("Connecting failed (wifi status %u) after %u ms, create an AP instead", WiFi.status(), (millis() - tMillis));
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

  AsyncResponseStream *response = request->beginResponseStream("text/html");
  printHTMLTop(response);

  if (isApMode()) {
    response->printf(htmlWifiApConnectForm.c_str());
  } else {
    response->printf("<p>Connected to '%s'</p>", SSID);
  }

  char mqttDataType[256];
  snprintf(mqttDataType, 256, "<option %s value='0'>JSON - single message</option>\
    <option %s value='1'>Scalar - each value a different message</option>\
    <option %s value='2'>Both</option>", 
    configuration.mqttDataType == MQTT_DATA_JSON ? "selected" : "", 
    configuration.mqttDataType == MQTT_DATA_SCALAR ? "selected" : "", 
    configuration.mqttDataType == MQTT_DATA_BOTH ? "selected" : "");
  
  response->printf(htmlDeviceConfigs.c_str(), configuration.name,  
    configuration.mqttServer, configuration.mqttPort, configuration.mqttTopic, mqttDataType);

  printHTMLBottom(response);
  request->send(response);
}

void CWifiManager::handleConnect(AsyncWebServerRequest *request) {

  Log.infoln("handleConnect");

  String ssid = request->arg("ssid");
  String password = request->arg("password");
  
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  
  printHTMLTop(response);
  response->printf("<p>Connecting to '%s' ... see you on the other side!</p>", ssid.c_str());
  printHTMLBottom(response);

  request->send(response);

  ssid.toCharArray(configuration.wifiSsid, sizeof(configuration.wifiSsid));
  password.toCharArray(configuration.wifiPassword, sizeof(configuration.wifiPassword));

  Log.noticeln("Saved config SSID: '%s'", configuration.wifiSsid);

  EEPROM_saveConfig();

  strcpy(SSID, configuration.wifiSsid);
  connect();
}

void CWifiManager::handleConfig(AsyncWebServerRequest *request) {

  Log.infoln("handleConfig");

  String deviceName = request->arg("deviceName");
  deviceName.toCharArray(configuration.name, sizeof(configuration.name));
  Log.infoln("Device req name: %s", deviceName);
  Log.infoln("Device size %i name: %s", sizeof(configuration.name), configuration.name);

  String mqttServer = request->arg("mqttServer");
  mqttServer.toCharArray(configuration.mqttServer, sizeof(configuration.mqttServer));
  Log.infoln("MQTT Server: %s", mqttServer);

  uint16_t mqttPort = atoi(request->arg("mqttPort").c_str());
  configuration.mqttPort = mqttPort;
  Log.infoln("MQTT Port: %u", mqttPort);

  String mqttTopic = request->arg("mqttTopic");
  mqttTopic.toCharArray(configuration.mqttTopic, sizeof(configuration.mqttTopic));
  Log.infoln("MQTT Topic: %s", mqttTopic);

  uint16_t mqttDataType = atoi(request->arg("mqttDataType").c_str());
  configuration.mqttDataType = mqttDataType;
  Log.infoln("MQTT Data Type: %u", mqttDataType);

  EEPROM_saveConfig();
  
  rebootNeeded = true;
  request->redirect("/");
}

void CWifiManager::handleFactoryReset(AsyncWebServerRequest *request) {
  Log.infoln("handleFactoryReset");
  
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->setCode(200);
  response->printf("OK");

  EEPROM_wipe();
  rebootNeeded = true;
  
  request->send(response);
}

void CWifiManager::postSensorUpdate() {

  if (!mqtt.connected()) {
    if (mqtt.state() < MQTT_CONNECTED 
      && strlen(configuration.mqttServer) && strlen(configuration.mqttTopic)) { // Reconnectable
      Log.noticeln("Attempting to reconnect from MQTT state %i at '%s:%i' ...", mqtt.state(), configuration.mqttServer, configuration.mqttPort);
      if (mqtt.connect(String(getDeviceId()).c_str())) {
        Log.noticeln("MQTT reconnected");
        sprintf_P(mqttSubcribeTopicConfig, "%s/%u/config", configuration.mqttTopic, getDeviceId());
        bool r = mqtt.subscribe(mqttSubcribeTopicConfig);
        Log.noticeln("Subscribed for config changes to MQTT topic '%s' success = %T", mqttSubcribeTopicConfig, r);
      } else {
        Log.warningln("MQTT reconnect failed, rc=%i", mqtt.state());
      }
    }
    if (!mqtt.connected()) {
      Log.noticeln("MQTT not connected %i", mqtt.state());
      return;
    }
  }

  if (!strlen(configuration.mqttTopic)) {
    Log.warningln("Blank MQTT topic");
    return;
  }

  char topic[255];
  bool current;
  float v; int iv;

  bool pJson = configuration.mqttDataType == MQTT_DATA_JSON || configuration.mqttDataType == MQTT_DATA_BOTH;
  bool pScalar = configuration.mqttDataType == MQTT_DATA_SCALAR || configuration.mqttDataType == MQTT_DATA_BOTH;

  if (!isApMode()) {
    iv = dBmtoPercentage(WiFi.RSSI());
    if (pScalar) {
      sprintf_P(topic, "%s/wifi_percent", configuration.mqttTopic);
      mqtt.publish(topic,String(iv).c_str());
      Log.noticeln("Sent '%i%' WiFI signal to MQTT topic '%s'", iv, topic);
    }
    if (pJson) {
      postMQTTJson["wifi_percent"] = iv;
      postMQTTJson["wifi_rssi"] = WiFi.RSSI();
    }
  }

  postedUpdate = true;

  time_t now; 
  time(&now);

  if (pScalar) {
    sprintf_P(topic, "%s/timestamp", configuration.mqttTopic);
    mqtt.publish(topic,String(now).c_str());
    Log.noticeln("Sent '%u' timestamp to MQTT topic '%s'", (unsigned long)now, topic);

    //sprintf_P(topic, "%s/apmode", configuration.mqttTopic);
    //mqtt.publish(topic,String(isApMode()).c_str());
    //Log.noticeln("Sent '%i' AP mode to MQTT topic '%s'", isApMode(), topic);

    sprintf_P(topic, "%s/uptime_millis", configuration.mqttTopic);
    mqtt.publish(topic,String(uptimeMillis).c_str());
    Log.noticeln("Sent '%ums' uptime to MQTT topic '%s'", uptimeMillis, topic);
  }

  if (pJson) {
    postMQTTJson["uptime_millis"] = uptimeMillis;
    // Convert to ISO8601 for JSON
    char buf[sizeof "2011-10-08T07:07:09Z"];
    strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
    postMQTTJson["timestamp_iso8601"] = String(buf);

    postMQTTJson["mqttConfigTopic"] = mqttSubcribeTopicConfig;

    // sensor Json
    sprintf_P(topic, "%s/json", configuration.mqttTopic);
    mqtt.beginPublish(topic, measureJson(postMQTTJson), false);
    BufferingPrint bufferedClient(mqtt, 32);
    serializeJson(postMQTTJson, bufferedClient);
    bufferedClient.flush();
    mqtt.endPublish();

    String jsonStr;
    serializeJson(postMQTTJson, jsonStr);
    Log.noticeln("Sent '%s' json to MQTT topic '%s'", jsonStr.c_str(), topic);
  }
}

bool CWifiManager::isApMode() { 
  return WiFi.getMode() == WIFI_AP; 
}

void CWifiManager::mqttCallback(char *topic, uint8_t *payload, unsigned int length) {

  if (length == 0) {
    return;
  }

  Log.noticeln("Received %u bytes message on MQTT topic '%s'", length, topic);
  if (!strcmp(topic, mqttSubcribeTopicConfig)) {
    deserializeJson(getMQTTJson, (const byte*)payload, length);

    String jsonStr;
    serializeJson(getMQTTJson, jsonStr);
    Log.noticeln("Received configuration over MQTT with json: '%s'", jsonStr.c_str());

    if (getMQTTJson.containsKey("name")) {
      strncpy(configuration.name, getMQTTJson["name"], 128);
    }

    if (getMQTTJson.containsKey("mqttTopic")) {
      strncpy(configuration.mqttTopic, getMQTTJson["mqttTopic"], 64);
    }

    // Delete the config message in case it was retained
    mqtt.publish(mqttSubcribeTopicConfig, NULL, 0, true);
    Log.noticeln("Deleted config message");

    EEPROM_saveConfig();
    postSensorUpdate();
  }
  
}

void CWifiManager::printHTMLTop(Print *p) {
  p->printf(htmlTop.c_str(), configuration.name, configuration.name, "");
}

void CWifiManager::printHTMLBottom(Print *p) {
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  p->printf(htmlBottom.c_str(), String(DEVICE_NAME), hr, min % 60, sec % 60, dBmtoPercentage(WiFi.RSSI()));
}