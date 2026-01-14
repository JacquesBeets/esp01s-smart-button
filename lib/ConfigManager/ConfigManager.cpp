#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

const char* ConfigManager::CONFIG_FILE = "/config.json";

ConfigManager::ConfigManager() {
    setDefaults();
}

void ConfigManager::setDefaults() {
    strlcpy(config.mqtt_broker, "192.168.0.112", sizeof(config.mqtt_broker));
    strlcpy(config.mqtt_port, "1883", sizeof(config.mqtt_port));
    strlcpy(config.mqtt_username, "", sizeof(config.mqtt_username));
    strlcpy(config.mqtt_password, "", sizeof(config.mqtt_password));
}

void ConfigManager::begin() {
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS, formatting...");
        LittleFS.format();
        LittleFS.begin();
    }
    load();
}

bool ConfigManager::load() {
    Serial.println("Loading config...");

    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("No config file found, using defaults");
        return false;
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        Serial.println("Failed to open config file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.println("Failed to parse config file");
        return false;
    }

    if (doc["mqtt_broker"]) {
        strlcpy(config.mqtt_broker, doc["mqtt_broker"], sizeof(config.mqtt_broker));
    }
    if (doc["mqtt_port"]) {
        strlcpy(config.mqtt_port, doc["mqtt_port"], sizeof(config.mqtt_port));
    }
    if (doc["mqtt_username"]) {
        strlcpy(config.mqtt_username, doc["mqtt_username"], sizeof(config.mqtt_username));
    }
    if (doc["mqtt_password"]) {
        strlcpy(config.mqtt_password, doc["mqtt_password"], sizeof(config.mqtt_password));
    }

    Serial.println("Config loaded:");
    Serial.print("  MQTT Broker: ");
    Serial.println(config.mqtt_broker);
    Serial.print("  MQTT Port: ");
    Serial.println(config.mqtt_port);

    return true;
}

bool ConfigManager::save() {
    Serial.println("Saving config...");

    JsonDocument doc;
    doc["mqtt_broker"] = config.mqtt_broker;
    doc["mqtt_port"] = config.mqtt_port;
    doc["mqtt_username"] = config.mqtt_username;
    doc["mqtt_password"] = config.mqtt_password;

    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    serializeJson(doc, file);
    file.close();
    Serial.println("Config saved");
    return true;
}

void ConfigManager::reset() {
    Serial.println("Resetting config...");
    LittleFS.remove(CONFIG_FILE);
    setDefaults();
}

void ConfigManager::setMqttBroker(const char* value) {
    strlcpy(config.mqtt_broker, value, sizeof(config.mqtt_broker));
}

void ConfigManager::setMqttPort(const char* value) {
    strlcpy(config.mqtt_port, value, sizeof(config.mqtt_port));
}

void ConfigManager::setMqttUsername(const char* value) {
    strlcpy(config.mqtt_username, value, sizeof(config.mqtt_username));
}

void ConfigManager::setMqttPassword(const char* value) {
    strlcpy(config.mqtt_password, value, sizeof(config.mqtt_password));
}
