# ESP32-C6 OTA Update System

A complete implementation for over-the-air (OTA) firmware updates on ESP32-C6 devices using WiFi, MQTT, and HTTPS.

## Overview

This project implements a robust OTA update mechanism for ESP32-C6 microcontrollers, allowing firmware to be remotely updated without physical access to the device. The system uses MQTT for update notifications and HTTPS for secure firmware download.

## Features

- **WiFi Connectivity**: Automatic connection and reconnection to configured networks
- **MQTT Client**: Subscribes to update notification topics
- **Secure Updates**: HTTPS download with certificate validation
- **Update Verification**: Validates firmware integrity before installation
- **Rollback Protection**: Automatic rollback to previous firmware on failed updates
- **Progress Reporting**: Real-time update progress indicators

## Prerequisites

- ESP-IDF v5.0 or later
- An MQTT broker for update notifications
- An HTTPS server to host firmware files
- A CA certificate for server validation

## Configuration

The following parameters can be configured in the code:

- WiFi credentials (SSID and password)
- MQTT broker URL, credentials, and topic
- Firmware server URL
- Update check intervals

## Building and Flashing

```bash
# Clone the repository
git clone https://github.com/username/esp32_ota_update.git
cd esp32_ota_update

# Configure the project
idf.py menuconfig

# Build the project
idf.py build

# Flash to ESP32-C6
idf.py -p PORT flash monitor
```

## Update Process

1. The ESP32 connects to WiFi and subscribes to the MQTT update topic
2. When an update is available, publish a message to the topic with:
   ```json
   {
     "version": "1.0.1",
     "url": "https://your-server.com/firmware/esp32_ota_update.bin"
   }
   ```
3. The device will:
   - Compare the version with the current version
   - Download the firmware if an update is needed
   - Verify and install the update
   - Reboot into the new firmware

## Security Considerations

- Always use HTTPS for firmware downloads
- Validate certificates to prevent man-in-the-middle attacks
- Implement version checking to prevent downgrade attacks
- Use secure MQTT connections when possible

## License

MIT License
