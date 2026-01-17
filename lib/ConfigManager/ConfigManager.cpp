#include "ConfigManager.h"
#include <LittleFS.h>

const char* ConfigManager::CONFIG_FILE = "/config.json";

ConfigManager::ConfigManager() {
    setDefaults();
}

void ConfigManager::setDefaults() {
    strlcpy(config.mqtt_broker, "192.168.0.112", sizeof(config.mqtt_broker));
    strlcpy(config.mqtt_port, "1883", sizeof(config.mqtt_port));
    strlcpy(config.mqtt_username, "beetsdebeermqtt", sizeof(config.mqtt_username));
    strlcpy(config.mqtt_password, "831126", sizeof(config.mqtt_password));
}

void ConfigManager::begin() {
    if (!LittleFS.begin()) {
        LittleFS.format();
        LittleFS.begin();
    }
    load();
}

// Simple JSON value extraction - avoids ArduinoJson dependency (~20KB savings)
static bool extractJsonValue(const String& json, const char* key, char* dest, size_t destSize) {
    String searchKey = String("\"") + key + "\":\"";
    int start = json.indexOf(searchKey);
    if (start < 0) return false;

    start += searchKey.length();
    int end = json.indexOf('"', start);
    if (end < 0) return false;

    String value = json.substring(start, end);
    strlcpy(dest, value.c_str(), destSize);
    return true;
}

bool ConfigManager::load() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        return false;
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        return false;
    }

    String json = file.readString();
    file.close();

    extractJsonValue(json, "mqtt_broker", config.mqtt_broker, sizeof(config.mqtt_broker));
    extractJsonValue(json, "mqtt_port", config.mqtt_port, sizeof(config.mqtt_port));
    extractJsonValue(json, "mqtt_username", config.mqtt_username, sizeof(config.mqtt_username));
    extractJsonValue(json, "mqtt_password", config.mqtt_password, sizeof(config.mqtt_password));

    return true;
}

bool ConfigManager::save() {
    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) {
        return false;
    }

    // Manual JSON construction
    file.print("{");
    file.print("\"mqtt_broker\":\""); file.print(config.mqtt_broker); file.print("\",");
    file.print("\"mqtt_port\":\""); file.print(config.mqtt_port); file.print("\",");
    file.print("\"mqtt_username\":\""); file.print(config.mqtt_username); file.print("\",");
    file.print("\"mqtt_password\":\""); file.print(config.mqtt_password); file.print("\"");
    file.print("}");
    file.close();
    return true;
}

void ConfigManager::reset() {
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
