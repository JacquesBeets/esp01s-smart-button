#include <Arduino.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <OTA.h>
#include "credentials.h"

AsyncWebServer server(80);
AsyncEventSource events("/events");

// Define button pins
const int BUTTON1_PIN = 0;  // GPIO0
const int BUTTON2_PIN = 2;  // GPIO2

// Variables to store button states
int lastButton1State = HIGH;
int lastButton2State = HIGH;

// Debounce time in milliseconds
const unsigned long DEBOUNCE_DELAY = 50;

// Last time the buttons were checked
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;

int lastSteadyState1 = HIGH;
int lastSteadyState2 = HIGH;
int lastFlickerableState1 = HIGH;
int lastFlickerableState2 = HIGH;

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>SMART BUTTON</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0; background-color: #2f4468; color: white;}
    .topnav { overflow: hidden; background-color: #2f4468; color: white; font-size: 1.7rem; }
    .content { padding: 20px; text-align: center; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .packet { color: #bebebe; }
    .card.temperature { color: #fd7e14; }
    .card.humidity { color: #1b78e2; }
    #serial { height: 500px; overflow: scroll; background-color: #2f4468; color: white; font-size: 1.2rem; padding: 10px;}
  </style>
</head>
<body>
  <div class="topnav">
    <h3>SMART BUTTON</h3>
  </div>
  <div class="content">
    <button id="button1">Button 1</button>
    <button id="button2">Button 2</button>
  </div>
  <div id="serial">
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected", e);
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);

 source.addEventListener('serial', function(e) {
  console.log("serial message: ", e.data);
  document.getElementById("serial").innerHTML = document.getElementById("serial").innerHTML + '<br/>' + e.data;
 }, false);
 
}

function sendMessage(message) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log("Message sent");
    }
  };
  xhttp.open("POST", "/toggle", true);
  xhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
  xhttp.send("message=" + message); 
}

window.onload = function() {
  document.getElementById("button1").addEventListener("click", function() {
    sendMessage("button1");
  });   
  document.getElementById("button2").addEventListener("click", function() {
    sendMessage("button2");
  });   
}
</script>
</body>
</html>)rawliteral";

void serialToWeb (String message) {
  events.send(message.c_str(), "serial", millis());
}

void switchHub(String button) {
    serialToWeb("Switching Hub - Button " + button + " pressed");
    // events.send(button.c_str(), "button_press", millis());
}

static unsigned long lastEventTime = millis();
static const unsigned long EVENT_INTERVAL_MS = 5000;
void handlePushEventsPing() { 
    if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
        events.send("ping", NULL, millis());
        lastEventTime = millis();
    }
}

void setup() {
    Serial.begin(115200);

    setupOTA("SmartButton");

    // Set button pins as inputs with pull-up resistors
    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);

    // Setup web server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    server.on("/toggle", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("message", true)) {
            String message = request->getParam("message", true)->value();
            switchHub(message);
        }
        request->send(200, "text/plain", "OK");
    });

    // Events 
    events.onConnect([](AsyncEventSourceClient *client){
        if(client->lastId()){
            Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        }
        client->send("hello!", NULL, millis(), 10000);
    });

    server.addHandler(&events);
    server.onNotFound(notFound);
    server.begin();
}

void loop() {
    handlePushEventsPing();
    ArduinoOTA.handle();

    // Read button states
    int currentState1 = digitalRead(BUTTON1_PIN);
    int currentState2 = digitalRead(BUTTON2_PIN);

    // Button 1 logic
    if (currentState1 != lastFlickerableState1) {
        lastDebounceTime1 = millis();
        lastFlickerableState1 = currentState1;
        serialToWeb("Button 1 state changed. Current state: " + String(currentState1));
    }

    if ((millis() - lastDebounceTime1) > DEBOUNCE_DELAY) {
        if (lastSteadyState1 == HIGH && currentState1 == LOW) {
            serialToWeb("Button 1 pressed (after debounce)");
            switchHub("1");
        } else if (lastSteadyState1 == LOW && currentState1 == HIGH) {
            serialToWeb("Button 1 released (after debounce)");
        }
        lastSteadyState1 = currentState1;
    }

    // Button 2 logic (similar to Button 1)
    if (currentState2 != lastFlickerableState2) {
        lastDebounceTime2 = millis();
        lastFlickerableState2 = currentState2;
        serialToWeb("Button 2 state changed. Current state: " + String(currentState2));
    }

    if ((millis() - lastDebounceTime2) > DEBOUNCE_DELAY) {
        if (lastSteadyState2 == HIGH && currentState2 == LOW) {
            serialToWeb("Button 2 pressed (after debounce)");
            switchHub("2");
        } else if (lastSteadyState2 == LOW && currentState2 == HIGH) {
            serialToWeb("Button 2 released (after debounce)");
        }
        lastSteadyState2 = currentState2;
    }
}