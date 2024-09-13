#include "MQTTManager.h"

MQTTManager::MQTTManager(const char* broker, int port, const char* username, const char* password, const char* clientId)
    : _broker(broker), _port(port), _username(username), _password(password), _clientId(clientId), _client(_espClient) {
    _client.setServer(_broker, _port);
}

void MQTTManager::connect() {
    while (!_client.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (_client.connect(_clientId, _username, _password)) {
            Serial.println("Connected to MQTT broker");
        } else {
            Serial.print("Failed to connect to MQTT broker, rc=");
            Serial.print(_client.state());
            Serial.println(" Retrying in 5 seconds");
            delay(5000);
        }
    }
}

bool MQTTManager::isConnected() {
    return _client.connected();
}

void MQTTManager::loop() {
    _client.loop();
}

void MQTTManager::publish(const char* topic, const char* message) {
    _client.publish(topic, message);
}