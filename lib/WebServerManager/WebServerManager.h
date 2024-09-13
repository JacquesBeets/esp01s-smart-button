// WebServerManager.h
#pragma once
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

class WebServerManager {
public:
    WebServerManager(int port = 80);
    void begin();
    void addEventSource(const char* url);
    void sendEvent(const char* message, const char* event = "message");
    void onNotFound(ArRequestHandlerFunction fn);
    void on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction fn);
    void serveStatic(const char* uri, const char* path);

private:
    AsyncWebServer _server;
    AsyncEventSource* _events;
};

