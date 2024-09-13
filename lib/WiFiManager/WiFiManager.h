#pragma once
#include <ESP8266WiFi.h>

class WiFiManager {
public:
    WiFiManager(const char* ssid, const char* password);
    void connect();
    bool isConnected();
    String getIPAddress();

private:
    const char* _ssid;
    const char* _password;
};