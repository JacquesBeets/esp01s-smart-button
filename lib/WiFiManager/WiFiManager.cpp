#include "WiFiManager.h"

WiFiManager::WiFiManager(const char* ssid, const char* password, const char* apSsid)
    : _ssid(ssid), _password(password), _apSsid(apSsid) {
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
        return true;
    } else {
        Serial.println("\nFailed to connect to WiFi");
        return false;
    }
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
}

void WiFiManager::setupServer() {
    _server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<html><body>"
                      "<h1>WiFi Setup</h1>"
                      "<form action='/save' method='POST'>"
                      "SSID: <input type='text' name='ssid'><br>"
                      "Password: <input type='password' name='pass'><br>"
                      "<input type='submit' value='Save'>"
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

    _server->onNotFound([](AsyncWebServerRequest *request){
        request->redirect("/");
    });
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIPAddress() {
    return WiFi.localIP().toString();
}