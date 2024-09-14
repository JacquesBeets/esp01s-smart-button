#include <Arduino.h>
#include <ArduinoJson.h>
#include "OTA.h"
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "ButtonManager.h"
#include "WebServerManager.h"
#include "HtmlTemplates.h"
#include "credentials.h"

// WiFi setup
const char* wifi_ssid = WIFI_SSID;
const char* wifi_password = WIFI_PASSWORD;
WiFiManager wifiManager(wifi_ssid, wifi_password, "SmartButton_AP");

// Unique ID
char uidPrefix[] = "beetssmtbtn";
char devUniqueID[30];

// MQTT setup
const char* MQTT_CLIENT_ID = "ESP01s_SmartButton";
const char* MQTT_DISCOVERY_PREFIX = "homeassistant";
const char* MQTT_NODE_ID;
const char* MQTT_BUTTON1_ID = "button1";
const char* MQTT_BUTTON2_ID = "button2";


MQTTManager mqttManager(MQTT_BROKER, MQTT_PORT, ENV_MQTT_USERNAME, ENV_MQTT_PASSWORD, MQTT_CLIENT_ID);

const int BUTTON1_PIN = 0;  // GPIO0
const int BUTTON2_PIN = 2;  // GPIO2
ButtonManager button1(BUTTON1_PIN);
ButtonManager button2(BUTTON2_PIN);

// Web server setup
WebServerManager webServer;
OTAManager otaManager("SmartButton");

unsigned long lastMqttReconnectAttempt = 0;


void createDiscoveryUniqueID() {
    strcpy(devUniqueID, uidPrefix);
    char* macAddr = wifiManager.getMacAddress();
    Serial.print("MAC Address: ");
    Serial.println(macAddr);
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
}

void handleButtonPress(const char* button_id) {
    createDiscoveryUniqueID();
    String stateTopic = String(MQTT_DISCOVERY_PREFIX) + "/" + MQTT_NODE_ID + "/" + button_id + "/state";
    mqttManager.publish(stateTopic.c_str(), "PRESS");
    serialToWeb("Button " + String(button_id) + " pressed");
}

void publishDiscoveryMessage(const char* button_id) {
    createDiscoveryUniqueID();
    String discoveryTopic = String(MQTT_DISCOVERY_PREFIX) + "/device_automation/" + MQTT_NODE_ID + "_" + button_id + "/config";
    serialToWeb(discoveryTopic);
    DynamicJsonDocument doc(1024);
    const size_t CAPACITY = JSON_ARRAY_SIZE(1);
    StaticJsonDocument<CAPACITY> docArr;
    JsonArray array = docArr.to<JsonArray>();
    array.add(MQTT_NODE_ID);
    doc["automation_type"] = "trigger";
    doc["topic"] = String(MQTT_DISCOVERY_PREFIX) + "/" + MQTT_NODE_ID + "/" + button_id + "/state";
    doc["type"] = "button_short_press";
    doc["subtype"] = button_id;
    doc["payload"] = "PRESS";
    doc["device"]["identifiers"] = docArr;
    doc["device"]["name"] = "Smart Button";
    doc["device"]["model"] = "ESP01s Smart Button";
    doc["device"]["manufacturer"] = "DIY";

    String output;
    serializeJson(doc, output);
    serialToWeb(output);
    mqttManager.publish(discoveryTopic.c_str(), output.c_str(), true);
}

void unDescover(const char* button_id) {
    createDiscoveryUniqueID();
    String discoveryTopic = String(MQTT_DISCOVERY_PREFIX) + "/device_automation/" + MQTT_NODE_ID + "_" + button_id + "/config";
    serialToWeb("Un-Descover: " + discoveryTopic);
    mqttManager.publish(discoveryTopic.c_str(), NULL);
}

void setup() {
    Serial.begin(115200);
    otaManager.begin();
    wifiManager.begin();
    
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    webServer.on("/upload", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", upload_html);
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
        request->send(200, "text/plain", "OK");
    });

    webServer.on("/descover", HTTP_POST, [](AsyncWebServerRequest *request){
        publishDiscoveryMessage(MQTT_BUTTON1_ID);
        publishDiscoveryMessage(MQTT_BUTTON2_ID);
        request->send(200, "text/plain", "OK");
    });

    webServer.on("/un-descover", HTTP_POST, [](AsyncWebServerRequest *request){
        unDescover(MQTT_BUTTON1_ID);
        unDescover(MQTT_BUTTON2_ID);
        request->send(200, "text/plain", "OK");
    });

    webServer.addEventSource("/events");
    webServer.onNotFound(notFound);
    webServer.begin();

    Serial.println("Web server started");
}

void loop() {
    otaManager.handle();
    wifiManager.handleClient();

    button1.update();
    button2.update();

    if (button1.isPressed()) {
        handleButtonPress(MQTT_BUTTON1_ID);
    }
    if (button2.isPressed()) {
        handleButtonPress(MQTT_BUTTON2_ID);
    }

    // Non-blocking MQTT connection
    if (!mqttManager.isConnected()) {
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt > 5000) {
            lastMqttReconnectAttempt = now;
            if (mqttManager.connect()) {
                Serial.println("MQTT reconnected");
                // publishDiscoveryMessage(MQTT_BUTTON1_ID);
                // publishDiscoveryMessage(MQTT_BUTTON2_ID);
            }
        }
    } else {
        mqttManager.loop();
    }

    static unsigned long lastEventTime = millis();
    static const unsigned long EVENT_INTERVAL_MS = 5000;
    if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
        webServer.sendEvent("ping");
        lastEventTime = millis();
    }
}