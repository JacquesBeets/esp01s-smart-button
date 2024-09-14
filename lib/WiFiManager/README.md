# WiFiManager with OTA Support

This WiFiManager class provides an easy way to manage WiFi connections and perform OTA (Over-The-Air) updates for ESP8266-based projects. It includes a fallback AP (Access Point) mode for when the device can't connect to the configured WiFi network.

## Features

- Automatic connection to configured WiFi network
- Fallback AP mode if WiFi connection fails
- Web interface for WiFi configuration
- OTA update support in both WiFi and AP modes

## Usage

1. Include the WiFiManager in your project:

```cpp
#include "WiFiManager.h"
```

2. Create a WiFiManager instance in your code:

```cpp
WiFiManager wifiManager(wifi_ssid, wifi_password, "YourDeviceName_AP");
```

3. In your `setup()` function, initialize the WiFiManager:

```cpp
void setup() {
    // ... other setup code ...
    wifiManager.begin();
    // ... rest of your setup code ...
}
```

4. In your `loop()` function, call the `handleClient()` method:

```cpp
void loop() {
    wifiManager.handleClient();
    // ... rest of your loop code ...
}
```

## Connecting to WiFi

The device will automatically attempt to connect to the configured WiFi network on startup. If the connection is successful, it will operate normally.

## Fallback AP Mode

If the device fails to connect to the configured WiFi network, it will enter AP mode:

1. The device will create a WiFi network with the SSID you specified (e.g., "YourDeviceName_AP").
2. Connect to this network from your computer or smartphone.
3. Open a web browser and navigate to `http://192.168.4.1`.
4. You will see a page with options to configure WiFi or perform an OTA update.

## Configuring WiFi

1. On the web interface, enter the SSID and password for your WiFi network.
2. Click "Save".
3. The device will restart and attempt to connect to the new network.

## Performing OTA Updates

You can perform OTA updates whether the device is connected to WiFi or in AP mode:

1. If in AP mode, connect to the device's AP and navigate to `http://192.168.4.1`.
   If connected to WiFi, navigate to the device's IP address.
2. Scroll down to the "OTA Update" section.
3. Click "Choose File" and select the new firmware binary.
4. Click "Update" to start the OTA process.
5. Wait for the update to complete. The device will restart automatically.

## Troubleshooting

- If the device is stuck in a loop trying to connect to WiFi, it will eventually enter AP mode.
- In AP mode, you can always access the configuration page at `http://192.168.4.1`.