#pragma once
#include <Arduino.h>

// Config structure
struct Config {
    char mqtt_broker[64];
    char mqtt_port[6];
    char mqtt_username[32];
    char mqtt_password[32];
};

class ConfigManager {
public:
    ConfigManager();
    void begin();
    bool load();
    bool save();
    void reset();

    Config config;

    // Convenience getters
    const char* getMqttBroker() { return config.mqtt_broker; }
    int getMqttPort() { return atoi(config.mqtt_port); }
    const char* getMqttUsername() { return config.mqtt_username; }
    const char* getMqttPassword() { return config.mqtt_password; }

    // Setters
    void setMqttBroker(const char* value);
    void setMqttPort(const char* value);
    void setMqttUsername(const char* value);
    void setMqttPassword(const char* value);

private:
    static const char* CONFIG_FILE;
    void setDefaults();
};
