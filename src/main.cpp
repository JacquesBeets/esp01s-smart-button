#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include "ConfigManager.h"
#include "MQTTManager.h"
#include "ButtonManager.h"

// =============================================================================
// Debug Macros - Serial output disabled in release builds to save flash
// =============================================================================
#ifdef DEBUG_BUILD
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

// =============================================================================
// Configuration
// =============================================================================

ConfigManager configManager;
WiFiManager wifiManager;
bool shouldSaveConfig = false;

// WiFiManager custom parameters
WiFiManagerParameter* custom_mqtt_broker;
WiFiManagerParameter* custom_mqtt_port;
WiFiManagerParameter* custom_mqtt_username;
WiFiManagerParameter* custom_mqtt_password;

// =============================================================================
// Device Identity
// =============================================================================

char uidPrefix[] = "beetssmtbtn";
char devUniqueID[30];
const char* MQTT_CLIENT_ID = "ESP01s_SmartButton";
const char* MQTT_DISCOVERY_PREFIX = "homeassistant";
const char* MQTT_NODE_ID = nullptr;  // Initialized by createDiscoveryUniqueID()
const char* MQTT_BUTTON1_ID = "button1";
const char* MQTT_BUTTON2_ID = "button2";

// =============================================================================
// Hardware
// =============================================================================

const int BUTTON1_PIN = 0;  // GPIO0
const int BUTTON2_PIN = 2;  // GPIO2
ButtonManager button1(BUTTON1_PIN);
ButtonManager button2(BUTTON2_PIN);

// =============================================================================
// Managers
// =============================================================================

MQTTManager mqttManager;
ESP8266WebServer webServer(80);

// =============================================================================
// Timing
// =============================================================================

unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
bool discoveryPublished = false;  // Track if HA discovery has been sent

// =============================================================================
// Callbacks
// =============================================================================

void saveConfigCallback() {
    DEBUG_PRINTLN("Should save config");
    shouldSaveConfig = true;
}

// =============================================================================
// Utility Functions
// =============================================================================

void createDiscoveryUniqueID() {
    byte macAddr[6];
    WiFi.macAddress(macAddr);

    strcpy(devUniqueID, uidPrefix);
    int preSizeBytes = sizeof(uidPrefix);
    int j = 0;
    for (int i = 2; i >= 0; i--) {
        sprintf(&devUniqueID[(preSizeBytes - 1) + (j)], "%02X", macAddr[i]);
        j = j + 2;
    }
    DEBUG_PRINT("Unique ID: ");
    DEBUG_PRINTLN(devUniqueID);
    MQTT_NODE_ID = devUniqueID;
}

// =============================================================================
// Button Handling
// =============================================================================

void handleButtonPress(const char* button_id) {
    createDiscoveryUniqueID();
    String stateTopic = String(MQTT_DISCOVERY_PREFIX) + "/" + MQTT_NODE_ID + "/" + button_id + "/state";
    mqttManager.publish(stateTopic.c_str(), "PRESS");
    DEBUG_PRINTLN("Button " + String(button_id) + " pressed");
}

// =============================================================================
// Home Assistant Discovery
// =============================================================================

void publishDiscoveryMessage(const char* button_id) {
    createDiscoveryUniqueID();
    String discoveryTopic = String(MQTT_DISCOVERY_PREFIX) + "/device_automation/" + MQTT_NODE_ID + "_" + button_id + "/config";
    DEBUG_PRINTLN("Publishing discovery: " + discoveryTopic);

    // Manual JSON construction - saves ~20KB by removing ArduinoJson dependency
    String stateTopic = String(MQTT_DISCOVERY_PREFIX) + "/" + MQTT_NODE_ID + "/" + button_id + "/state";

    String json = "{";
    json += "\"automation_type\":\"trigger\",";
    json += "\"topic\":\"" + stateTopic + "\",";
    json += "\"type\":\"button_short_press\",";
    json += "\"subtype\":\"" + String(button_id) + "\",";
    json += "\"payload\":\"PRESS\",";
    json += "\"device\":{";
    json += "\"identifiers\":[\"" + String(MQTT_NODE_ID) + "\"],";
    json += "\"name\":\"Smart Button\",";
    json += "\"model\":\"ESP01s Smart Button\",";
    json += "\"manufacturer\":\"DIY\"";
    json += "}}";

    mqttManager.publish(discoveryTopic.c_str(), json.c_str(), true);
    DEBUG_PRINTLN("Discovery published");
}

void unDiscover(const char* button_id) {
    createDiscoveryUniqueID();
    String discoveryTopic = String(MQTT_DISCOVERY_PREFIX) + "/device_automation/" + MQTT_NODE_ID + "_" + button_id + "/config";
    DEBUG_PRINTLN("Removing discovery: " + discoveryTopic);
    mqttManager.publish(discoveryTopic.c_str(), "");
}

// =============================================================================
// Web Server Handlers
// =============================================================================

void handleRoot() {
    // Minified HTML - saves ~1-2KB flash
    String html = F("<!DOCTYPE html><html><head><title>Smart Button</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:Arial;text-align:center;background:#1a1a1a;color:#fff;padding:20px}"
        ".b{background:#4CAF50;border:none;color:#fff;padding:15px 30px;margin:5px;cursor:pointer;border-radius:4px;font-size:16px}"
        ".r{background:#f44336}.u{background:#2196F3}.i{background:#333;padding:15px;border-radius:8px;margin:15px 0;text-align:left}</style></head><body>"
        "<h1>Smart Button</h1><div class='i'>");
    html += "<p><b>IP:</b> " + WiFi.localIP().toString() + "</p>";
    html += "<p><b>MQTT:</b> " + String(configManager.getMqttBroker()) + ":" + String(configManager.getMqttPort()) + "</p>";
    html += "<p><b>Status:</b> " + String(mqttManager.isConnected() ? "Connected" : "Disconnected") + "</p></div>";
    html += F("<h3>Test</h3><form action='/toggle' method='POST'>"
        "<button class='b' name='button' value='button1'>Btn 1</button>"
        "<button class='b' name='button' value='button2'>Btn 2</button></form>"
        "<h3>Home Assistant</h3><form action='/discover' method='POST' style='display:inline'>"
        "<button class='b u'>Add to HA</button></form>"
        "<form action='/undiscover' method='POST' style='display:inline'>"
        "<button class='b r'>Remove</button></form>"
        "<h3>Device</h3><form action='/reset' method='POST'>"
        "<button class='b r'>Reset WiFi</button></form></body></html>");
    webServer.send(200, "text/html", html);
}

void handleToggle() {
    if (webServer.hasArg("button")) {
        String button = webServer.arg("button");
        if (button == "button1") {
            handleButtonPress(MQTT_BUTTON1_ID);
        } else if (button == "button2") {
            handleButtonPress(MQTT_BUTTON2_ID);
        }
    }
    webServer.sendHeader("Location", "/");
    webServer.send(303);
}

void handleDiscover() {
    publishDiscoveryMessage(MQTT_BUTTON1_ID);
    publishDiscoveryMessage(MQTT_BUTTON2_ID);
    webServer.sendHeader("Location", "/");
    webServer.send(303);
}

void handleUndiscover() {
    unDiscover(MQTT_BUTTON1_ID);
    unDiscover(MQTT_BUTTON2_ID);
    webServer.sendHeader("Location", "/");
    webServer.send(303);
}

void handleReset() {
    webServer.send(200, "text/html", "<h1>Resetting...</h1><p>Connect to 'SmartButton-Setup' to reconfigure.</p>");
    delay(1000);
    wifiManager.resetSettings();
    configManager.reset();
    ESP.restart();
}

void handleNotFound() {
    webServer.send(404, "text/plain", "Not found");
}

// =============================================================================
// WiFi Setup
// =============================================================================

void setupWiFiManager() {
    WiFi.hostname("smart-button");

    // Create custom parameters with current values
    custom_mqtt_broker = new WiFiManagerParameter("mqtt_broker", "MQTT Broker", configManager.getMqttBroker(), 64);
    custom_mqtt_port = new WiFiManagerParameter("mqtt_port", "MQTT Port", configManager.config.mqtt_port, 6);
    custom_mqtt_username = new WiFiManagerParameter("mqtt_user", "MQTT Username", configManager.getMqttUsername(), 32);
    custom_mqtt_password = new WiFiManagerParameter("mqtt_pass", "MQTT Password", configManager.getMqttPassword(), 32);

    wifiManager.addParameter(custom_mqtt_broker);
    wifiManager.addParameter(custom_mqtt_port);
    wifiManager.addParameter(custom_mqtt_username);
    wifiManager.addParameter(custom_mqtt_password);

    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.setConfigPortalTimeout(180);
    wifiManager.setMinimumSignalQuality(20);

    DEBUG_PRINTLN("Connecting to WiFi...");
    if (!wifiManager.autoConnect("SmartButton-Setup")) {
        DEBUG_PRINTLN("Failed to connect, restarting...");
        delay(3000);
        ESP.restart();
    }

    DEBUG_PRINTLN("WiFi connected!");
    DEBUG_PRINT("IP address: ");
    DEBUG_PRINTLN(WiFi.localIP());

    // Read updated parameters
    configManager.setMqttBroker(custom_mqtt_broker->getValue());
    configManager.setMqttPort(custom_mqtt_port->getValue());
    configManager.setMqttUsername(custom_mqtt_username->getValue());
    configManager.setMqttPassword(custom_mqtt_password->getValue());

    if (shouldSaveConfig) {
        configManager.save();
    }
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
#ifdef DEBUG_BUILD
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== ESP01s Smart Button ===\n");
#endif

    // Load saved configuration
    configManager.begin();

    // Setup WiFi with captive portal
    setupWiFiManager();

    // Create unique device ID
    createDiscoveryUniqueID();

    // Setup ArduinoOTA for remote updates
    ArduinoOTA.setHostname("smart-button");
#ifdef DEBUG_BUILD
    ArduinoOTA.onStart([]() {
        Serial.println("OTA Update starting...");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA Update complete!");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
#endif
    ArduinoOTA.begin();
    DEBUG_PRINTLN("ArduinoOTA ready");

    // Configure MQTT with loaded settings
    mqttManager.configure(
        configManager.getMqttBroker(),
        configManager.getMqttPort(),
        configManager.getMqttUsername(),
        configManager.getMqttPassword(),
        MQTT_CLIENT_ID
    );

    // Web server routes
    webServer.on("/", HTTP_GET, handleRoot);
    webServer.on("/toggle", HTTP_POST, handleToggle);
    webServer.on("/discover", HTTP_POST, handleDiscover);
    webServer.on("/undiscover", HTTP_POST, handleUndiscover);
    webServer.on("/reset", HTTP_POST, handleReset);
    webServer.onNotFound(handleNotFound);
    webServer.begin();

    DEBUG_PRINTLN("Web server started");
    DEBUG_PRINTLN("\n=== Setup Complete ===\n");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
    // Handle OTA updates
    ArduinoOTA.handle();

    // Handle web server
    webServer.handleClient();

    // Update button states
    button1.update();
    button2.update();

    // Handle button presses
    if (button1.stateChanged() && button1.isPressed()) {
        handleButtonPress(MQTT_BUTTON1_ID);
    }
    if (button2.stateChanged() && button2.isPressed()) {
        handleButtonPress(MQTT_BUTTON2_ID);
    }

    // Non-blocking MQTT reconnect
    if (!mqttManager.isConnected()) {
        discoveryPublished = false;  // Reset so discovery is resent on reconnect
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
            lastMqttReconnectAttempt = now;
            mqttManager.connect();
        }
    } else {
        // Publish HA discovery on first connect
        if (!discoveryPublished) {
            delay(100);  // Brief delay to ensure MQTT connection is stable
            publishDiscoveryMessage(MQTT_BUTTON1_ID);
            publishDiscoveryMessage(MQTT_BUTTON2_ID);
            discoveryPublished = true;
        }
        mqttManager.loop();
    }
}
