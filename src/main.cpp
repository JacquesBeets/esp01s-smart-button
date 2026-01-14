#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "ConfigManager.h"
#include "MQTTManager.h"
#include "ButtonManager.h"
#include "WebServerManager.h"
#include "HtmlTemplates.h"

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
const char* MQTT_NODE_ID;
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
WebServerManager webServer;

// =============================================================================
// Timing
// =============================================================================

unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

// =============================================================================
// Callbacks
// =============================================================================

void saveConfigCallback() {
    Serial.println("Should save config");
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
    Serial.print("Unique ID: ");
    Serial.println(devUniqueID);
    MQTT_NODE_ID = devUniqueID;
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void serialToWeb(const String& message) {
    webServer.sendEvent(message.c_str(), "serial");
    Serial.println(message);
}

// =============================================================================
// Button Handling
// =============================================================================

void handleButtonPress(const char* button_id) {
    createDiscoveryUniqueID();
    String stateTopic = String(MQTT_DISCOVERY_PREFIX) + "/" + MQTT_NODE_ID + "/" + button_id + "/state";
    mqttManager.publish(stateTopic.c_str(), "PRESS");
    serialToWeb("Button " + String(button_id) + " pressed");
}

// =============================================================================
// Home Assistant Discovery
// =============================================================================

void publishDiscoveryMessage(const char* button_id) {
    createDiscoveryUniqueID();
    String discoveryTopic = String(MQTT_DISCOVERY_PREFIX) + "/device_automation/" + MQTT_NODE_ID + "_" + button_id + "/config";
    serialToWeb("Publishing discovery: " + discoveryTopic);

    JsonDocument doc;
    JsonArray ids = doc["device"]["identifiers"].to<JsonArray>();
    ids.add(MQTT_NODE_ID);

    doc["automation_type"] = "trigger";
    doc["topic"] = String(MQTT_DISCOVERY_PREFIX) + "/" + MQTT_NODE_ID + "/" + button_id + "/state";
    doc["type"] = "button_short_press";
    doc["subtype"] = button_id;
    doc["payload"] = "PRESS";
    doc["device"]["name"] = "Smart Button";
    doc["device"]["model"] = "ESP01s Smart Button";
    doc["device"]["manufacturer"] = "DIY";

    String output;
    serializeJson(doc, output);
    mqttManager.publish(discoveryTopic.c_str(), output.c_str(), true);
    serialToWeb("Discovery published");
}

void unDiscover(const char* button_id) {
    createDiscoveryUniqueID();
    String discoveryTopic = String(MQTT_DISCOVERY_PREFIX) + "/device_automation/" + MQTT_NODE_ID + "_" + button_id + "/config";
    serialToWeb("Removing discovery: " + discoveryTopic);
    mqttManager.publish(discoveryTopic.c_str(), "");
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

    Serial.println("Connecting to WiFi...");
    if (!wifiManager.autoConnect("SmartButton-Setup")) {
        Serial.println("Failed to connect, restarting...");
        delay(3000);
        ESP.restart();
    }

    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

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
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== ESP01s Smart Button ===\n");

    // Load saved configuration
    configManager.begin();

    // Setup WiFi with captive portal
    setupWiFiManager();

    // Create unique device ID
    createDiscoveryUniqueID();

    // Configure MQTT with loaded settings
    mqttManager.configure(
        configManager.getMqttBroker(),
        configManager.getMqttPort(),
        configManager.getMqttUsername(),
        configManager.getMqttPassword(),
        MQTT_CLIENT_ID
    );

    // Web server routes
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<html><head>";
        html += "<title>Smart Button</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>";
        html += "body{font-family:Arial;text-align:center;background:#1a1a1a;color:#fff;padding:20px;}";
        html += ".btn{background:#4CAF50;border:none;color:#fff;padding:15px 30px;margin:5px;cursor:pointer;border-radius:4px;font-size:16px;}";
        html += ".btn-red{background:#f44336;}";
        html += ".btn-blue{background:#2196F3;}";
        html += ".info{background:#333;padding:15px;border-radius:8px;margin:15px 0;text-align:left;}";
        html += "</style></head><body>";
        html += "<h1>Smart Button</h1>";

        html += "<div class='info'>";
        html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
        html += "<p><strong>MQTT:</strong> " + String(configManager.getMqttBroker()) + ":" + String(configManager.getMqttPort()) + "</p>";
        html += "<p><strong>MQTT Status:</strong> " + String(mqttManager.isConnected() ? "Connected" : "Disconnected") + "</p>";
        html += "</div>";

        html += "<h3>Test Buttons</h3>";
        html += "<form action='/toggle' method='POST'>";
        html += "<button class='btn' name='message' value='button1'>Button 1</button>";
        html += "<button class='btn' name='message' value='button2'>Button 2</button>";
        html += "</form>";

        html += "<h3>Home Assistant</h3>";
        html += "<form action='/discover' method='POST' style='display:inline'>";
        html += "<button class='btn btn-blue'>Add to HA</button></form>";
        html += "<form action='/undiscover' method='POST' style='display:inline'>";
        html += "<button class='btn btn-red'>Remove from HA</button></form>";

        html += "<h3>Device</h3>";
        html += "<form action='/reset' method='POST'>";
        html += "<button class='btn btn-red'>Reset WiFi Config</button></form>";

        html += "</body></html>";
        request->send(200, "text/html", html);
    });

    webServer.on("/toggle", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("message", true)) {
            String message = request->getParam("message", true)->value();
            if (message == "button1") {
                handleButtonPress(MQTT_BUTTON1_ID);
            } else if (message == "button2") {
                handleButtonPress(MQTT_BUTTON2_ID);
            }
        }
        request->redirect("/");
    });

    webServer.on("/discover", HTTP_POST, [](AsyncWebServerRequest *request){
        publishDiscoveryMessage(MQTT_BUTTON1_ID);
        publishDiscoveryMessage(MQTT_BUTTON2_ID);
        request->redirect("/");
    });

    webServer.on("/undiscover", HTTP_POST, [](AsyncWebServerRequest *request){
        unDiscover(MQTT_BUTTON1_ID);
        unDiscover(MQTT_BUTTON2_ID);
        request->redirect("/");
    });

    webServer.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<h1>Resetting...</h1><p>Connect to 'SmartButton-Setup' to reconfigure.</p>");
        delay(1000);
        wifiManager.resetSettings();
        configManager.reset();
        ESP.restart();
    });

    webServer.addEventSource("/events");
    webServer.onNotFound(notFound);
    webServer.begin();

    Serial.println("Web server started");
    Serial.println("\n=== Setup Complete ===\n");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
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
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
            lastMqttReconnectAttempt = now;
            mqttManager.connect();
        }
    } else {
        mqttManager.loop();
    }

    // Periodic ping to web clients
    static unsigned long lastEventTime = millis();
    if ((millis() - lastEventTime) > 5000) {
        webServer.sendEvent("ping");
        lastEventTime = millis();
    }
}
