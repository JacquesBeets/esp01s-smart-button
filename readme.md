# ESP01S Smart Button

A DIY smart button built with an ESP-01S that sends button presses to Home Assistant via MQTT. Features two GPIO-based buttons with Home Assistant auto-discovery.

## Features

- **WiFiManager** - Web-based WiFi configuration with captive portal
- **MQTT Integration** - Send button press events to Home Assistant
- **Home Assistant Auto-Discovery** - Automatically appears as device triggers in HA
- **Web UI** - Test buttons and manage device via browser
- **Non-blocking MQTT** - Device remains responsive even if MQTT is down
- **LittleFS Config Storage** - Settings persist across reboots

## Hardware

- **ESP-01S** (ESP8266, 1MB flash)
- **GPIO0** - Button 1 (directly connected, active LOW)
- **GPIO2** - Button 2 (directly connected, active LOW)

## First-Time Setup

1. **Flash the firmware** via USB-to-Serial adapter
2. **Connect to WiFi AP** - Look for `SmartButton-Setup` network
3. **Open captive portal** - Should open automatically, or go to `http://192.168.4.1`
4. **Configure settings**:
   - WiFi SSID and password
   - MQTT Broker IP (e.g., `192.168.0.112`)
   - MQTT Port (default: `1883`)
   - MQTT Username (optional)
   - MQTT Password (optional)
5. **Save** - Device will restart and connect to your network

## Web Interface

Once connected, access the device at its IP address:

| Endpoint | Description |
|----------|-------------|
| `http://<device-ip>/` | Main web UI - status, test buttons, HA discovery |
| `http://<device-ip>/toggle` | POST endpoint to trigger buttons |
| `http://<device-ip>/discover` | POST endpoint to publish HA discovery |
| `http://<device-ip>/undiscover` | POST endpoint to remove from HA |
| `http://<device-ip>/reset` | POST endpoint to reset WiFi/MQTT config |

## MQTT Topics

Button presses are published to:
```
homeassistant/<device-unique-id>/button1/state  -> "PRESS"
homeassistant/<device-unique-id>/button2/state  -> "PRESS"
```

## Home Assistant Integration

The device supports MQTT auto-discovery for device triggers. To add to Home Assistant:

1. Ensure MQTT integration is configured in HA
2. Access the device web UI
3. Click **"Add to HA"** button
4. The buttons will appear as device triggers in automations

### Example Automation

```yaml
automation:
  - alias: "Smart Button 1 Press"
    trigger:
      - platform: device
        domain: mqtt
        device_id: <your-device-id>
        type: button_short_press
        subtype: button1
    action:
      - service: light.toggle
        target:
          entity_id: light.living_room
```

## Building & Flashing

### Prerequisites
- [PlatformIO](https://platformio.org/) (VSCode extension or CLI)
- USB-to-Serial adapter (for initial flash)

### Initial Flash (USB)

The ESP-01S requires a USB-to-Serial adapter with 3.3V logic levels.

1. Connect adapter:
   - VCC -> 3.3V
   - GND -> GND
   - TX -> RX
   - RX -> TX
   - GPIO0 -> GND (for flash mode)

2. Power cycle the ESP-01S while GPIO0 is grounded

3. Run: `pio run --target upload`

4. Disconnect GPIO0 from GND and power cycle

### OTA Updates (After Initial Setup)

Once the device is on your network, you can update via OTA by uncommenting these lines in `platformio.ini`:
```ini
upload_port = smart-button.local
upload_protocol = espota
```

## Resetting Configuration

If you need to reconfigure WiFi or MQTT settings:

**Option 1:** Access `http://<device-ip>/reset` from the web UI

**Option 2:** Erase flash and re-upload:
```bash
pio run --target erase --target upload
```

## Troubleshooting

### Device not creating WiFi AP
- Old credentials may be stored in flash
- Erase flash completely: `pio run --target erase --target upload`

### Can't connect to MQTT
- Check broker IP and port in web UI
- Verify MQTT credentials
- MQTT status shown on main web page

### Buttons not responding
- Check wiring - buttons should pull GPIO to GND when pressed
- GPIO0 and GPIO2 need pull-up resistors (internal pull-ups are enabled)

## Project Structure

```
esp01s-smart-button/
├── src/
│   └── main.cpp              # Main application
├── lib/
│   ├── ButtonManager/        # Button debouncing
│   ├── ConfigManager/        # LittleFS config storage
│   ├── MQTTManager/          # MQTT client wrapper
│   └── WebServerManager/     # Async web server wrapper
├── include/
│   └── HtmlTemplates.h       # HTML templates (legacy)
└── platformio.ini            # Build configuration
```

## License

MIT
