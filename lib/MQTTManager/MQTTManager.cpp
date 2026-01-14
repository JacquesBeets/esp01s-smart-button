#include "MQTTManager.h"

MQTTManager::MQTTManager() : _client(_espClient), _configured(false), _port(1883) {
    _broker[0] = '\0';
    _username[0] = '\0';
    _password[0] = '\0';
    _clientId[0] = '\0';
}

void MQTTManager::configure(const char* broker, int port, const char* username, const char* password, const char* clientId) {
    strlcpy(_broker, broker, sizeof(_broker));
    _port = port;
    strlcpy(_username, username, sizeof(_username));
    strlcpy(_password, password, sizeof(_password));
    strlcpy(_clientId, clientId, sizeof(_clientId));

    _client.setServer(_broker, _port);
    _client.setBufferSize(512);
    _configured = true;

    Serial.print("MQTT configured: ");
    Serial.print(_broker);
    Serial.print(":");
    Serial.println(_port);
}

bool MQTTManager::connect() {
    if (!_configured) {
        Serial.println("MQTT not configured");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Cannot attempt MQTT connection.");
        return false;
    }

    if (!_client.connected()) {
        Serial.print("Attempting MQTT connection to ");
        Serial.print(_broker);
        Serial.print(":");
        Serial.println(_port);

        bool connected;
        if (strlen(_username) > 0) {
            connected = _client.connect(_clientId, _username, _password);
        } else {
            connected = _client.connect(_clientId);
        }

        if (connected) {
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
    if (_client.connected()) {
        _client.publish(topic, message, retained);
    }
}
