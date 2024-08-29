#include <Arduino.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <OTA.h>
#include "credentials.h"

// Web server and event source setup
AsyncWebServer server(80);
AsyncEventSource events("/events");

// Button configuration
const int BUTTON1_PIN = 0;  // GPIO0
const int BUTTON2_PIN = 2;  // GPIO2
const unsigned long DEBOUNCE_DELAY = 50;

// Button state variables
struct ButtonState {
    int lastState;
    int lastSteadyState;
    int lastFlickerableState;
    unsigned long lastDebounceTime;
};

ButtonState button1 = {HIGH, HIGH, HIGH, 0};
ButtonState button2 = {HIGH, HIGH, HIGH, 0};

// Function prototypes
void notFound(AsyncWebServerRequest *request);
void serialToWeb(String message);
void switchHub(String button);
void handlePushEventsPing();
void handleButton(int pin, ButtonState &state, const String &buttonName);

// HTML content
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>SMART BUTTON</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {font-size: 1.2rem;}
    body {margin: 0; background-color: #2f4468; color: white;}
    .topnav {overflow: hidden; background-color: #2f4468; color: white; font-size: 1.7rem;}
    .content {padding: 20px; text-align: center;}
    .card {background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);}
    .cards {max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));}
    .reading {font-size: 2.8rem;}
    .packet {color: #bebebe;}
    .card.temperature {color: #fd7e14;}
    .card.humidity {color: #1b78e2;}
    #serial {height: 500px; overflow: scroll; background-color: #2f4468; color: white; font-size: 1.2rem; padding: 10px;}
  </style>
</head>
<body>
  <div class="topnav"><h3>SMART BUTTON</h3></div>
  <div class="content">
    <button id="button1">Button 1</button>
    <button id="button2">Button 2</button>
  </div>
  <div id="serial"></div>
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
  document.getElementById("serial").innerHTML += '<br/>' + e.data;
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

void setup() {
    Serial.begin(115200);
    setupOTA("SmartButton");

    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);

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
    handleButton(BUTTON1_PIN, button1, "1");
    handleButton(BUTTON2_PIN, button2, "2");
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void serialToWeb(String message) {
    events.send(message.c_str(), "serial", millis());
}

void switchHub(String button) {
    serialToWeb("Switching Hub - Button " + button + " pressed");
}

void handlePushEventsPing() {
    static unsigned long lastEventTime = millis();
    static const unsigned long EVENT_INTERVAL_MS = 5000;
    if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
        events.send("ping", NULL, millis());
        lastEventTime = millis();
    }
}

void handleButton(int pin, ButtonState &state, const String &buttonName) {
    int currentState = digitalRead(pin);

    if (currentState != state.lastFlickerableState) {
        state.lastDebounceTime = millis();
        state.lastFlickerableState = currentState;
    }

    if ((millis() - state.lastDebounceTime) > DEBOUNCE_DELAY) {
        if (state.lastSteadyState == HIGH && currentState == LOW) {
            switchHub(buttonName);
        }
        state.lastSteadyState = currentState;
    }
    
    state.lastState = currentState;
}