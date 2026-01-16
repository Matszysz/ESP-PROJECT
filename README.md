# IoT Environmental & Location Tracker (ESP32 CYD)

## 📖 Project Abstract
This project is an embedded IoT system designed to monitor geographical location and environmental parameters in real-time. Built on the **ESP32-2432S028 (Cheap Yellow Display)** platform, the device integrates satellite positioning (GPS) with cloud connectivity to visualize data both locally (on a touchscreen) and remotely via the **Google Firebase** cloud infrastructure.

The system serves as a prototype for mobile environmental monitoring stations, capable of tracking routes and mapping sensor data (e.g., PM2.5 levels) to specific coordinates.

## 🏗️ System Architecture

The system consists of three main layers:
1.  **Hardware Layer (Edge Device):**
    * Collects NMEA data from GPS satellites.
    * Generates environmental data (Simulation Mode for Air Quality).
    * Renders a graphical user interface (GUI) using the LVGL library.
2.  **Communication Layer:**
    * Establishes a secure WiFi connection (WPA2).
    * Serializes data into JSON format.
3.  **Cloud Layer (Backend):**
    * **Google Firebase Realtime Database:** Stores telemetry data.
    * Provides an endpoint for external applications (maps, dashboards) to fetch live data.

### Data Flow Diagram
```mermaid
graph TD;
    Sat[GPS Satellites] -->|NMEA Signals| GPS[NEO-6M Module];
    GPS -->|UART Serial| ESP[ESP32 Core];
    Env[Environment Data] -->|Simulation| ESP;
    ESP -->|SPI Bus| LCD[TFT Display / GUI];
    ESP -->|WiFi / HTTPS| Cloud[Google Firebase DB];