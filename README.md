# ESP32-CYD Environmental Monitoring Station 🌍📉

**Project Type:** Engineering Thesis / IoT Prototype
**Device:** ESP32-2432S028R ("Cheap Yellow Display" - CYD)
**Firmware Version:** 1.0 (Stable / HTTP Variant)

## 📖 Project Overview
This project is an IoT Environmental Station designed to monitor real-time weather conditions and detailed air quality metrics. It utilizes the ESP32 microcontroller with a built-in TFT touchscreen to present data via a modern **LVGL** graphical interface.

The device fetches data from multiple APIs based on a specific location (Gdańsk, Poland) and uploads processed metrics to the **ThingSpeak** cloud for long-term storage and analysis.

### Key Features
* **Real-time Monitoring:** Displays Temperature, Pressure, PM2.5, PM10, NO2, SO2, O3, and CO.
* **Dual-Cloud Architecture:** Uses *Open-Meteo* for data acquisition and *ThingSpeak* for data logging.
* **Status Indication:** Rear RGB LED indicates connection status (Red=Error, Blue=Syncing, Green=Success).
* **Data Export:** Collected data can be downloaded from the cloud in **CSV, JSON, or XML** formats.

---

## ⚙️ Engineering Decisions: The Migration from Firebase to ThingSpeak

During the development phase, a critical architectural change was made regarding the cloud backend.

### 1. The Initial Approach (Firebase)
Initially, the project utilized **Google Firebase (RTDB)**. While powerful, it requires **SSL/TLS encryption** for every connection.
* **The Problem:** The ESP32-WROOM chip has limited RAM. The heavy SSL buffers (required for encryption) conflicted with the memory-intensive **LVGL graphics library**. This caused Heap Fragmentation and "Guru Meditation Error" (Core Panic) crashes during WiFi negotiation.

### 2. The Solution (ThingSpeak)
To ensure system stability without upgrading hardware (e.g., to ESP32-S3 with PSRAM), the backend was migrated to **ThingSpeak**.
* **Why:** ThingSpeak supports standard **HTTP (port 80)** requests, which are significantly lighter on memory than HTTPS.
* **Result:** This eliminated the "Out of Memory" crashes, allowing the rich GUI and network stack to run simultaneously with perfect stability.

---

## 🛠️ Hardware Specifications

| Component | Model | Function |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32-WROOM-32 | Core logic and Wi-Fi connectivity |
| **Display** | 2.8" ILI9341 | 320x240 TFT LCD for data visualization |
| **Touch Controller** | XPT2046 | Resistive touch input handling |
| **RGB LED** | SMD (Built-in) | Visual status feedback (Pins 4, 16, 17) |
| **GPS Module** | u-blox NEO-6M | *Hardware support included, currently disabled for indoor demo stability* |

---

## 💻 Software & Architecture

The firmware is developed using **PlatformIO** (VS Code).

### Data Flow Diagram
1.  **ESP32** wakes up every 60 seconds.
2.  **Request 1:** Connects to `api.open-meteo.com` to fetch Weather (Temp/Pressure).
3.  **Request 2:** Connects to `air-quality-api.open-meteo.com` to fetch Pollutants (PM2.5, NO2, etc.).
    * *Note:* These are split into two requests to ensure data integrity and avoid "zero value" errors caused by different API endpoints.
4.  **Processing:** Updates the LVGL GUI elements (Bars, Labels).
5.  **Upload:** Pushes key metrics to **ThingSpeak Cloud**.

### Key Libraries
* **LVGL 9.1:** Advanced graphics engine for the UI.
* **TFT_eSPI:** High-speed driver for the display.
* **ArduinoJson v7:** Efficient parsing of API responses.
* **HTTPClient:** Lightweight web requests.

---

## 📊 Cloud & Data Analysis (ThingSpeak)

The project allows for advanced data analysis. The ThingSpeak channel collects data points every minute.

### Data Visualization
The platform provides real-time charts for Temperature, Pressure, and Air Quality indices.

### Data Export Options
One of the key features of this system is the ability to export historical data for research purposes. Data can be downloaded directly from the channel in the following formats:
* **CSV (Comma Separated Values):** For Excel/MATLAB analysis.
* **JSON:** For web application integration.
* **XML:** For legacy systems.

---

## ⚠️ Challenges & Solutions

| Challenge | Solution |
| :--- | :--- |
| **RAM Exhaustion** | Switched from Firebase (SSL) to ThingSpeak (HTTP) and optimized JSON buffers. |
| **GPS Indoor Signal** | Implemented a "Hardcoded Fallback" mode. Coordinates are set to Gdańsk (54.35, 18.64) to ensure data availability during indoor presentations. |
| **Air Quality = 0** | Discovered that Open-Meteo separates Weather and Air Quality into different API endpoints. Split the logic into two distinct HTTP GET requests. |
| **Flash Memory Lock** | Applied `IRAM_ATTR` to the LVGL timer to prevent crashes during WiFi SPI operations. |

---

## 🚀 Installation

1.  **Prerequisites:** VS Code with PlatformIO extension.
2.  **Configuration:**
    * Open `src/main.cpp`.
    * Edit `ssid` and `password` for your WiFi.
    * Paste your **ThingSpeak Write API Key**.
3.  **Partition Scheme:** Ensure `board_build.partitions = huge_app.csv` is set in `platformio.ini`.
4.  **Upload:** Connect via USB and flash the firmware.

---

**Author:** Group2
**Date:** January 2026
**Location:** Gdańsk, Poland