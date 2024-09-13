#include "WebServerManager.h"

WebServerManager::WebServerManager(int port) : _server(port), _events(nullptr) {}

void WebServerManager::begin() {
    _server.begin();
}

void WebServerManager::addEventSource(const char* url) {
    _events = new AsyncEventSource(url);
    _server.addHandler(_events);
}

void WebServerManager::sendEvent(const char* message, const char* event) {
    if (_events) {
        _events->send(message, event, millis());
    }
}

void WebServerManager::onNotFound(ArRequestHandlerFunction fn) {
    _server.onNotFound(fn);
}

void WebServerManager::on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction fn) {
    _server.on(uri, method, fn);
}

void WebServerManager::serveStatic(const char* uri, const char* path) {
    _server.serveStatic(uri, LittleFS, path);
}