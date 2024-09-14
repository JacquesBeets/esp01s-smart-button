#include "MQTTManager.h"

MQTTManager::MQTTManager(const char* broker, int port, const char* username, const char* password, const char* clientId)
    : _broker(broker), _port(port), _username(username), _password(password), _clientId(clientId), _client(_espClient) {
    _client.setServer(_broker, _port);
    _client.setBufferSize(512);
}

bool MQTTManager::connect() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Cannot attempt MQTT connection.");
        return false;
    }
    
    if (!_client.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (_client.connect(_clientId, _username, _password)) {
            Serial.println("Connected to MQTT broker");
            return true;
        } else {
            Serial.print("Failed to connect to MQTT broker, rc=");
            Serial.println(_client.state());
            return false;
        }
    }
    return true;
}

bool MQTTManager::isConnected() {
    return _client.connected();
}

void MQTTManager::loop() {
    _client.loop();
}

void MQTTManager::publish(const char* topic, const char* message, boolean retained) {
    _client.publish(topic, message, retained);
}