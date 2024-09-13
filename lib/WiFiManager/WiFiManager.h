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
    void handleClient();  // Method to handle DNS in AP mode

private:
    const char* _ssid;
    const char* _password;
    const char* _apSsid;
    AsyncWebServer* _server;
    DNSServer _dnsServer;
    bool _apMode;
    
    void startAP();
    void setupServer();
    bool connectWiFi();
    void handleOTA(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
};