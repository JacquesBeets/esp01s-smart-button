#include <Arduino.h>
#include <ArduinoOTA.h>
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

// MQTT setup
const char* MQTT_CLIENT_ID = "ESP01s_SmartButton";
const char* MQTT_TOPIC_BUTTON1 = "office/jacques/smartbutton/button1/click";
const char* MQTT_TOPIC_BUTTON2 = "office/jacques/smartbutton/button2/click";
MQTTManager mqttManager(MQTT_BROKER, MQTT_PORT, ENV_MQTT_USERNAME, ENV_MQTT_PASSWORD, MQTT_CLIENT_ID);


const int BUTTON1_PIN = 0;  // GPIO0
const int BUTTON2_PIN = 2;  // GPIO2
ButtonManager button1(BUTTON1_PIN);
ButtonManager button2(BUTTON2_PIN);

// Web server setup
WebServerManager webServer;
OTAManager otaManager("SmartButton");

unsigned long lastMqttReconnectAttempt = 0;

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void serialToWeb(const String& message) {
    webServer.sendEvent(message.c_str(), "serial");
}

void switchHub(const String& button) {
    serialToWeb("Switching Hub - Button " + button + " pressed");
    if (button == "1") {
        mqttManager.publish(MQTT_TOPIC_BUTTON1, "pressed");
    } else if (button == "2") {
        mqttManager.publish(MQTT_TOPIC_BUTTON2, "pressed");
    }
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
            switchHub(message);
        }
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
        switchHub("1");
    }
    if (button2.isPressed()) {
        switchHub("2");
    }

    // Non-blocking MQTT connection
    if (!mqttManager.isConnected()) {
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt > 5000) {
            lastMqttReconnectAttempt = now;
            if (mqttManager.connect()) {
                Serial.println("MQTT reconnected");
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