# ESP32 CYD Environmental Tracker (IoT Project)

An engineering project utilizing the ESP32-2432S028 "Cheap Yellow Display" (CYD) to monitor geolocation and environmental data. The device enables real-time tracking via Google Firebase and visualizes data on a touch-enabled interface.

## 🚀 Key Features

* **Real-time GPS Tracking:** Integrates NEO-6M module to parse NMEA sentences.
* **Cloud Synchronization:** Pushes telemetry data (Lat/Lon/AirQuality) to Firebase Realtime Database.
* **Interactive GUI:** Built with **LVGL 9** library, featuring status indicators and tab navigation.
* **Robust Error Handling:** Implements custom protections against hardware brownouts and connection failures.

## 🔧 Technical Solutions (Engineering Challenges)

This project solves several critical issues common in ESP32 development:

### 1. Brownout Protection (Power Stability)
The ESP32 radio requires high current spikes during WiFi negotiation. To prevent the board from resetting due to voltage drops (Brownout Detector triggering):
* We manually disabled the Brownout Detector: `WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);`.
* We implemented a **"Safe Startup"** logic: WiFi initialization is delayed by 4 seconds to allow the screen and touch drivers to stabilize first.

### 2. Touchscreen Freeze Fix
High TX power on WiFi can interfere with the SPI bus used by the XPT2046 touch controller.
* **Solution:** The WiFi transmission power is limited to `WIFI_POWER_11dBm` (instead of the default 20dBm). This ensures the touch controller remains responsive during data transmission.

### 3. Memory Safety
Standard Arduino `String` concatenation can cause heap fragmentation and compilation errors with the Firebase library.
* **Solution:** We implemented a step-by-step string construction method (using operators `+=`) to ensure safe memory management and compiler compatibility.

## 🛠️ Hardware Requirements

1.  **ESP32-2432S028 (CYD):** The main controller with integrated ILI9341 display.
2.  **GY-GPS6MV2 (NEO-6M):** External GPS module.
3.  **Power Source:** High-quality 5V/2A USB adapter or Power Bank (Crucial for GPS fix).

### Wiring
| GPS Pin | ESP32 Pin |
| :--- | :--- |
| VCC | 5V / 3.3V |
| GND | GND |
| TX | GPIO 22 |
| RX | GPIO 27 |

## 💻 How to Run (For Group Members)

1.  **Clone the Repo:** Download this project to your computer.
2.  **VS Code + PlatformIO:** Ensure you have the PlatformIO extension installed.
3.  **Update Credentials:**
    * Open `src/secrets.h`.
    * If you are using a different Hotspot, update `WIFI_SSID` and `WIFI_PASSWORD`.
    * *Note: Firebase credentials are pre-configured for our group database.*
4.  **Upload:** Connect the board via USB and click "Upload".

## 📊 Database Schema
Data is stored in Firebase under the `/routes` node:
```json
{
  "timestamp_id": {
    "lat": 52.2297,
    "lng": 21.0122,
    "air": 45,    // Simulated PM2.5 value
    "ts": 150020  // Runtime millis
  }
}