#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ESP8266HTTPClient.h>
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

// Fixed STA network config (applied by WiFiManager before connecting)
const IPAddress STATIC_IP(192, 168, 0, 93);
const IPAddress STATIC_GATEWAY(192, 168, 0, 1);
const IPAddress STATIC_SUBNET(255, 255, 255, 0);
const IPAddress STATIC_DNS(192, 168, 0, 1);

// WiFiManager custom parameters
WiFiManagerParameter* custom_mqtt_broker;
WiFiManagerParameter* custom_mqtt_port;
WiFiManagerParameter* custom_mqtt_username;
WiFiManagerParameter* custom_mqtt_password;
WiFiManagerParameter* custom_hdmi_host;
WiFiManagerParameter* custom_usb_host;

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
const unsigned long MQTT_BACKOFF_MIN = 5000;    // First retry delay
const unsigned long MQTT_BACKOFF_MAX = 60000;   // Cap
unsigned long mqttBackoff = MQTT_BACKOFF_MIN;   // Doubles per consecutive failure
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

// HTTP timeouts. HTTPClient::setTimeout() is what bounds the TCP connect wait:
// HTTPClient::connect() copies it onto the WiFiClient (overriding client.setTimeout())
// right before WiFiClient::connect(), and it also bounds DNS lookup and the response read.
const uint16_t HTTP_TIMEOUT_FIRST_MS = 2000;   // First attempt on press
const uint16_t HTTP_TIMEOUT_RETRY_MS = 1000;   // Retries from loop()
const unsigned long LAN_RETRY_INTERVAL = 5000; // Per-target retry spacing
const unsigned long LAN_RETRY_WINDOW = 90000;  // Give up this long after the press

// Desired LAN state from the last press, plus per-target pending/retry tracking
uint8_t hdmiWant = 0;            // 1 or 2
const char* usbWant = "";        // "Mac" or "PC"
bool hdmiPending = false;
bool usbPending = false;
unsigned long lanPressMillis = 0;
unsigned long hdmiLastTry = 0;
unsigned long usbLastTry = 0;

// GET http://{host}{path}. Returns true on HTTP 200, or if host is empty (nothing to do).
bool httpGet(const char* host, const char* path, uint16_t timeoutMs) {
    if (host[0] == 0) return true;
    if (WiFi.status() != WL_CONNECTED) return false;
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(timeoutMs);
    http.setReuse(false);
    String url = String("http://") + host + path;
    if (!http.begin(client, url)) {
        DEBUG_PRINTLN("HTTP begin failed: " + url);
        return false;
    }
    int code = http.GET();
    DEBUG_PRINTF("GET %s -> %d\n", url.c_str(), code);
    http.end();
    return code == HTTP_CODE_OK;
}

bool sendHdmi(uint16_t timeoutMs) {
    return httpGet(configManager.getHdmiHost(), hdmiWant == 1 ? "/set?input=1" : "/set?input=2", timeoutMs);
}

bool sendUsb(uint16_t timeoutMs) {
    String path = String("/switch?to=") + usbWant;
    return httpGet(configManager.getUsbHost(), path.c_str(), timeoutMs);
}

// Drive the HDMI switch and USB switch directly over HTTP. Sets the desired state,
// attempts both now, and leaves failed targets pending for retryPendingLan().
void fireLanActions(const char* button_id) {
    bool isButton1 = (strcmp(button_id, MQTT_BUTTON1_ID) == 0);
    hdmiWant = isButton1 ? 1 : 2;
    usbWant = isButton1 ? "Mac" : "PC";
    lanPressMillis = millis();
    hdmiPending = !sendHdmi(HTTP_TIMEOUT_FIRST_MS);
    hdmiLastTry = millis();
    usbPending = !sendUsb(HTTP_TIMEOUT_FIRST_MS);
    usbLastTry = millis();
}

// Called from loop(): at most ONE HTTP attempt per call, each target spaced
// LAN_RETRY_INTERVAL apart, until 200 or LAN_RETRY_WINDOW after the press.
void retryPendingLan() {
    if (!hdmiPending && !usbPending) return;
    unsigned long now = millis();
    if (now - lanPressMillis > LAN_RETRY_WINDOW) {
        DEBUG_PRINTLN("LAN retry window expired");
        hdmiPending = usbPending = false;
        return;
    }
    if (hdmiPending && now - hdmiLastTry >= LAN_RETRY_INTERVAL) {
        hdmiPending = !sendHdmi(HTTP_TIMEOUT_RETRY_MS);
        hdmiLastTry = millis();
    } else if (usbPending && now - usbLastTry >= LAN_RETRY_INTERVAL) {
        usbPending = !sendUsb(HTTP_TIMEOUT_RETRY_MS);
        usbLastTry = millis();
    }
}

void handleButtonPress(const char* button_id) {
    DEBUG_PRINTLN("Button " + String(button_id) + " pressed");
    if (mqttManager.isConnected()) {
        createDiscoveryUniqueID();
        String stateTopic = String(MQTT_DISCOVERY_PREFIX) + "/" + MQTT_NODE_ID + "/" + button_id + "/state";
        mqttManager.publish(stateTopic.c_str(), "PRESS");
    }
    fireLanActions(button_id);
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
    html += "<p><b>Status:</b> " + String(mqttManager.isConnected() ? "Connected" : "Disconnected") + "</p>";
    html += "<p><b>HDMI:</b> " + String(configManager.getHdmiHost()) + "</p>";
    html += "<p><b>USB:</b> " + String(configManager.getUsbHost()) + "</p>";
    html += "<p><b>pending:</b> ";
    if (hdmiPending || usbPending) {
        if (hdmiPending) html += "hdmi=" + String(hdmiWant) + " ";
        if (usbPending) html += "usb=" + String(usbWant) + " ";
        unsigned long el = millis() - lanPressMillis;
        html += "(retrying, " + String(el < LAN_RETRY_WINDOW ? (LAN_RETRY_WINDOW - el) / 1000 : 0) + " s left)";
    } else {
        html += "none";
    }
    html += "</p></div>";
    html += F("<h3>Config</h3><form action='/config' method='POST'>");
    html += "<p>MQTT Broker <input name='mqtt_broker' maxlength='63' value='" + String(configManager.getMqttBroker()) + "'></p>";
    html += "<p>MQTT Port <input name='mqtt_port' maxlength='5' value='" + String(configManager.config.mqtt_port) + "'></p>";
    html += "<p>HDMI Host <input name='hdmi_host' maxlength='63' value='" + String(configManager.getHdmiHost()) + "'></p>";
    html += "<p>USB Host <input name='usb_host' maxlength='63' value='" + String(configManager.getUsbHost()) + "'></p>";
    html += F("<button class='b'>Save</button></form>");
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
    String button = webServer.arg("button");
    // Respond first so the browser is not held up by the LAN GETs
    webServer.sendHeader("Location", "/");
    webServer.send(303);
    if (button == "button1") {
        handleButtonPress(MQTT_BUTTON1_ID);
    } else if (button == "button2") {
        handleButtonPress(MQTT_BUTTON2_ID);
    }
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

void applyMqttConfig() {
    mqttManager.configure(
        configManager.getMqttBroker(),
        configManager.getMqttPort(),
        configManager.getMqttUsername(),
        configManager.getMqttPassword(),
        MQTT_CLIENT_ID
    );
    discoveryPublished = false;
    mqttBackoff = MQTT_BACKOFF_MIN;
    lastMqttReconnectAttempt = millis() - MQTT_BACKOFF_MIN;  // Retry on next loop
}

// Save config from the root-page form and re-apply MQTT live (no reboot)
void handleConfig() {
    if (webServer.hasArg("mqtt_broker")) configManager.setMqttBroker(webServer.arg("mqtt_broker").c_str());
    if (webServer.hasArg("mqtt_port")) configManager.setMqttPort(webServer.arg("mqtt_port").c_str());
    if (webServer.hasArg("hdmi_host")) configManager.setHdmiHost(webServer.arg("hdmi_host").c_str());
    if (webServer.hasArg("usb_host")) configManager.setUsbHost(webServer.arg("usb_host").c_str());
    configManager.save();
    applyMqttConfig();
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
    custom_hdmi_host = new WiFiManagerParameter("hdmi_host", "HDMI Switch Host", configManager.getHdmiHost(), 63);
    custom_usb_host = new WiFiManagerParameter("usb_host", "USB Switch Host", configManager.getUsbHost(), 63);

    wifiManager.addParameter(custom_mqtt_broker);
    wifiManager.addParameter(custom_mqtt_port);
    wifiManager.addParameter(custom_mqtt_username);
    wifiManager.addParameter(custom_mqtt_password);
    wifiManager.addParameter(custom_hdmi_host);
    wifiManager.addParameter(custom_usb_host);

    wifiManager.setSTAStaticIPConfig(STATIC_IP, STATIC_GATEWAY, STATIC_SUBNET, STATIC_DNS);
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
    configManager.setHdmiHost(custom_hdmi_host->getValue());
    configManager.setUsbHost(custom_usb_host->getValue());

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
    applyMqttConfig();

    // Web server routes
    webServer.on("/", HTTP_GET, handleRoot);
    webServer.on("/toggle", HTTP_POST, handleToggle);
    webServer.on("/discover", HTTP_POST, handleDiscover);
    webServer.on("/undiscover", HTTP_POST, handleUndiscover);
    webServer.on("/config", HTTP_POST, handleConfig);
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

    // Retry LAN targets that did not return 200 (at most one HTTP attempt per pass)
    retryPendingLan();

    // Non-blocking MQTT reconnect (millis() backoff; skipped if no broker host)
    if (!mqttManager.isConfigured()) {
        return;
    }
    if (!mqttManager.isConnected()) {
        discoveryPublished = false;  // Reset so discovery is resent on reconnect
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt >= mqttBackoff) {
            lastMqttReconnectAttempt = now;
            if (mqttManager.connect()) {
                mqttBackoff = MQTT_BACKOFF_MIN;
            } else {
                mqttBackoff = min(mqttBackoff * 2, MQTT_BACKOFF_MAX);
                DEBUG_PRINTF("MQTT connect failed, next retry in %lu ms\n", mqttBackoff);
            }
        }
    } else {
        // Publish HA discovery on first connect
        if (!discoveryPublished) {
            publishDiscoveryMessage(MQTT_BUTTON1_ID);
            publishDiscoveryMessage(MQTT_BUTTON2_ID);
            discoveryPublished = true;
        }
        mqttManager.loop();
    }
}
