// MQTTManager.h
#pragma once
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

class MQTTManager {
public:
    MQTTManager(const char* broker, int port, const char* username, const char* password, const char* clientId);
    void connect();
    bool isConnected();
    void loop();
    void publish(const char* topic, const char* message);

private:
    PubSubClient _client;
    const char* _broker;
    int _port;
    const char* _username;
    const char* _password;
    const char* _clientId;
    WiFiClient _espClient;
};