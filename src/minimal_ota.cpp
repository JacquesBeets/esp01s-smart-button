/**
 * Minimal OTA Firmware for ESP01S (1MB Flash)
 *
 * This is a "stepping stone" firmware for two-step OTA updates on space-constrained
 * ESP-01S devices. When the main firmware grows too large to OTA update directly:
 *
 * 1. Upload this minimal firmware first via OTA (~250KB)
 * 2. This frees up flash space
 * 3. Then upload the full firmware via OTA (~570KB max)
 *
 * This firmware only provides WiFi connectivity and OTA capability.
 * WiFi credentials are automatically remembered by ESP8266 from previous connection.
 *
 * Build with: pio run -e esp01_1m_minimal_ota
 * Upload with: pio run -e esp01_1m_minimal_ota -t upload
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

// Status LED (GPIO2 - active LOW)
const int LED_PIN = 2;

void setup() {
    // Setup LED for status indication
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  // LED off

    // WiFi will auto-connect using stored credentials from previous firmware
    WiFi.mode(WIFI_STA);
    WiFi.hostname("smart-button");
    WiFi.begin();  // Connect using stored credentials

    // Wait for WiFi connection with LED blink
    while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_PIN, LOW);   // LED on
        delay(250);
        digitalWrite(LED_PIN, HIGH);  // LED off
        delay(250);
    }

    // Setup ArduinoOTA - minimal config, no callbacks to save space
    ArduinoOTA.setHostname("smart-button");
    ArduinoOTA.begin();

    // Solid LED indicates ready for OTA
    digitalWrite(LED_PIN, LOW);
}

void loop() {
    ArduinoOTA.handle();

    // Blink LED slowly while running to indicate minimal OTA mode
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 2000) {
        lastBlink = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    // Reconnect WiFi if disconnected
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
        delay(5000);
    }
}
