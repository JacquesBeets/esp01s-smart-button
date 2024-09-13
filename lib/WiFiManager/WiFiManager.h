#pragma once
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

class WiFiManager {
public:
    WiFiManager(const char* ssid, const char* password, const char* apSsid = "ESP_AP");
    void begin();
    bool isConnected();
    String getIPAddress();

private:
    const char* _ssid;
    const char* _password;
    const char* _apSsid;
    AsyncWebServer* _server;
    DNSServer _dnsServer;
    
    void startAP();
    void setupServer();
    bool connectWiFi();
};