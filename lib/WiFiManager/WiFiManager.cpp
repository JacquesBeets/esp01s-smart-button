#include "WiFiManager.h"
#include <Arduino.h>

WiFiManager::WiFiManager(const char* ssid, const char* password, const char* apSsid)
    : _ssid(ssid), _password(password), _apSsid(apSsid), _apMode(false) {
    _server = new AsyncWebServer(80);
}

void WiFiManager::begin() {
    if (!connectWiFi()) {
        startAP();
    }
}

bool WiFiManager::connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid, _password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        _apMode = false;
        return true;
    } else {
        Serial.println("\nFailed to connect to WiFi");
        return false;
    }
}

char* WiFiManager::getMacAddress() {
    byte macAddr[6];
    WiFi.macAddress(macAddr);
    char* macAddress = new char[18];
    sprintf(macAddress, "%02X:%02X:%02X:%02X:%02X:%02X", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    return macAddress;
}

void WiFiManager::startAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(_apSsid);
    
    _dnsServer.start(53, "*", WiFi.softAPIP());
    
    setupServer();
    
    _server->begin();
    
    Serial.println("AP mode started");
    Serial.print("AP SSID: ");
    Serial.println(_apSsid);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    _apMode = true;
}

void WiFiManager::setupServer() {
    _server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<html><body>"
                      "<h1>WiFi Setup</h1>"
                      "<form action='/save' method='POST'>"
                      "SSID: <input type='text' name='ssid'><br>"
                      "Password: <input type='password' name='pass'><br>"
                      "<input type='submit' value='Save'>"
                      "</form>"
                      "<h2>OTA Update</h2>"
                      "<form method='POST' action='/update' enctype='multipart/form-data'>"
                      "<input type='file' name='update'>"
                      "<input type='submit' value='Update'>"
                      "</form></body></html>";
        request->send(200, "text/html", html);
    });

    _server->on("/save", HTTP_POST, [this](AsyncWebServerRequest *request){
        String newSsid;
        String newPass;
        if (request->hasParam("ssid", true)) {
            newSsid = request->getParam("ssid", true)->value();
        }
        if (request->hasParam("pass", true)) {
            newPass = request->getParam("pass", true)->value();
        }
        
        _ssid = newSsid.c_str();
        _password = newPass.c_str();
        
        request->send(200, "text/plain", "Credentials saved. Rebooting...");
        delay(1000);
        ESP.restart();
    });

    _server->on("/update", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", (Update.hasError()) ? "Update fail" : "Update success");
            ESP.restart();
        },
        [this](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
            this->handleOTA(request, filename, index, data, len, final);
        }
    );

    _server->onNotFound([](AsyncWebServerRequest *request){
        request->redirect("/");
    });
}

void WiFiManager::handleOTA(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.println("OTA update started");
        Update.runAsync(true);
        if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
            Update.printError(Serial);
        }
    }
    if (!Update.hasError()) {
        if (Update.write(data, len) != len) {
            Update.printError(Serial);
        }
    }
    if (final) {
        if (Update.end(true)) {
            Serial.println("OTA update finished");
        } else {
            Update.printError(Serial);
        }
    }
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIPAddress() {
    return WiFi.localIP().toString();
}

void WiFiManager::handleClient() {
    if (_apMode) {
        _dnsServer.processNextRequest();
    }
}