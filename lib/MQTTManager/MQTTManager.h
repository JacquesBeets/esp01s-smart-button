// MQTTManager.h
#pragma once
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

class MQTTManager {
public:
    MQTTManager();
    void configure(const char* broker, int port, const char* username, const char* password, const char* clientId);
    bool connect();
    bool isConnected();
    void loop();
    void publish(const char* topic, const char* message, boolean retained = false);

private:
    WiFiClient _espClient;   // MUST be declared before _client (initialization order)
    PubSubClient _client;
    char _broker[64];
    int _port;
    char _username[32];
    char _password[32];
    char _clientId[32];
    bool _configured;
};
