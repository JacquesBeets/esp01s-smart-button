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
    _client.setBufferSize(768);  // Increased for HA discovery payloads
    _configured = true;
}

bool MQTTManager::connect() {
    if (!_configured) {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    if (!_client.connected()) {
        bool connected;
        if (strlen(_username) > 0) {
            connected = _client.connect(_clientId, _username, _password);
        } else {
            connected = _client.connect(_clientId);
        }
        return connected;
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
