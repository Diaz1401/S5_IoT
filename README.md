# TRASI IoT - Water Quality Monitoring System

TRASI is an IoT-based shrimp pond water quality monitoring system built on the ESP32 platform. It continuously measures key water parameters—Turbidity, TDS (Total Dissolved Solids), pH, and Temperature—and logs the data to Firebase Realtime Database for remote monitoring and analysis.

## Features

*   **Multi-Sensor Monitoring**: Real-time reading of 4 critical water quality parameters.
*   **Cloud Integration**: Seamless data logging to Firebase Realtime Database.
*   **Secure Connectivity**: Uses SSL/TLS for secure communication.
*   **Time Synchronization**: Automatic NTP time syncing for accurate data timestamping.
*   **Demo Mode**: Built-in simulation mode for testing without hardware sensors.
*   **Dual Data Storage**: Maintains both a historical log and a real-time snapshot in the database.

## Hardware Requirements

*   **Microcontroller**: ESP32 DOIT DevKit v1
*   **Sensors**:
    *   **Turbidity Sensor** (Analog)
    *   **TDS Sensor** (Analog)
    *   **pH Sensor** (Analog)
    *   **DS18B20 Temperature Sensor** (OneWire)

### Wiring Diagram / Pinout

| Sensor | ESP32 Pin | Type | Note |
| :--- | :--- | :--- | :--- |
| **Turbidity** | GPIO 32 | Analog | |
| **TDS** | GPIO 33 | Analog | |
| **pH** | GPIO 34 | Analog | |
| **Temperature** | GPIO 4 | Digital | Requires 4.7kΩ pull-up resistor |

## Software Requirements

*   **VS Code** with **PlatformIO** extension.
*   **Framework**: Arduino for ESP32.

### Dependencies
The project relies on the following libraries (automatically managed by PlatformIO):
*   `mobizt/FirebaseClient`
*   `paulstoffregen/OneWire`
*   `milesburton/DallasTemperature`

## Installation & Setup

1.  **Clone the Repository**
    ```bash
    git clone https://github.com/Diaz1401/S5_IoT.git trasi
    cd trasi
    ```

2.  **Configure Secrets**
    Create a file named `secret.h` in the `src/` directory. This file is git-ignored to protect your credentials. Copy the following template and fill in your details:

    ```cpp
    // src/secret.h
    #ifndef SECRET_H
    #define SECRET_H

    // WiFi Credentials
    #define WIFI_SSID       "your-wifi-ssid"
    #define WIFI_PASSWORD   "your-wifi-password"

    // Firebase Credentials
    #define API_KEY         "your-firebase-api-key"
    #define DATABASE_URL    "your-project-id-default-rtdb.region.firebasedatabase.app"
    #define USER_EMAIL      "your-auth-email@example.com"
    #define USER_PASSWORD   "your-auth-password"

    // Operation Mode
    // Set to true to simulate sensor data (no hardware needed)
    const bool DEMO_MODE = false; 

    #endif
    ```

3.  **Build the Project**
    Open the project in VS Code (PlatformIO) and run:
    ```bash
    pio run
    ```

4.  **Upload to ESP32**
    Connect your ESP32 via USB and run:
    ```bash
    pio run --target upload
    ```

5.  **Monitor Output**
    To see serial logs and debug messages:
    ```bash
    pio device monitor
    ```
    *Note: Baud rate is set to **115200**.*

## Data Structure

The system organizes data in Firebase Realtime Database as follows:

*   **Historical Data**: `/sensorData/{user_uid}/{timestamp}`
    *   Stores a log of all readings indexed by unix timestamp.
*   **Realtime Snapshot**: `/sensorData/{user_uid}/realtime`
    *   Always contains the latest reading for quick access.

**JSON Payload Format:**
```json
{
  "temperature": 25.5,
  "turbidity": 12.4,
  "tds": 150.2,
  "ph": 7.1,
  "timestamp": 1700000000
}
```

## Calibration

Sensor calibration constants are defined at the top of `src/main.ino`. You may need to adjust these values based on your specific hardware calibration:

*   `tdsCalibration`: Multiplier for TDS value.
*   `phVoltageOffset`: Offset for pH sensor voltage.
*   `phSlope`: Slope for pH calculation.
