#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <TinyGPS++.h>
#include <Firebase_ESP_Client.h>


// --- BROWNOUT PROTECTION ---
// Disables the voltage drop detector to prevent reboot loops during WiFi connection
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ========================================== 
// 1. CONFIGURATION
// ==========================================
const char* ssid     = "********";    //fill in yours credentials
const char* password = "********"; 

#define API_KEY "AIzaSyCVEoqI3jtOk0RxIWA3iz3CQZ22bpIBLgw"
#define DATABASE_URL "https://esp32-group2-417e0-default-rtdb.europe-west1.firebasedatabase.app/" 

// ==========================================
// 2. HARDWARE PINS (CYD ESP32-2432S028)
// ==========================================
#define XPT_CS   33 
#define XPT_MOSI 32
#define XPT_MISO 39
#define XPT_CLK  25
#define RXD2 22
#define TXD2 27

XPT2046_Touchscreen touchscreen(XPT_CS); 
SPIClass touchSPI(HSPI);
TFT_eSPI tft = TFT_eSPI();
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
uint32_t draw_buf[SCREEN_WIDTH * SCREEN_HEIGHT / 10];

// UI Objects
lv_obj_t * label_status; 
lv_obj_t * label_wifi_icon;
lv_obj_t * label_gps;    
lv_obj_t * label_air;    

// Logic Flags
bool wifiStarted = false;
unsigned long startupTimer = 0;
bool firebaseReady = false;
unsigned long lastSend = 0;
int airQuality = 50; 

// ==========================================
// 3. TOUCHPAD DRIVER
// ==========================================
void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    if(touchscreen.touched()) {
        TS_Point p = touchscreen.getPoint();
        // Calibration (based on your device)
        int tx = map(p.x, 314, 3603, 0, SCREEN_WIDTH);
        int ty = map(p.y, 417, 3637, 0, SCREEN_HEIGHT);
        tx = constrain(tx, 0, SCREEN_WIDTH - 1);
        ty = constrain(ty, 0, SCREEN_HEIGHT - 1);
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = tx;
        data->point.y = ty;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)px_map, w*h, true);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

// ==========================================
// 4. GUI CREATION
// ==========================================
void create_gui() {
    // Red Cursor (Debug)
    lv_obj_t * cursor_obj = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cursor_obj, 10, 10);
    lv_obj_set_style_bg_color(cursor_obj, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_radius(cursor_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(cursor_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_indev_t * indev = lv_indev_get_next(NULL);
    if(indev) lv_indev_set_cursor(indev, cursor_obj);

    // Tabview
    lv_obj_t * tabview = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_obj_set_height(lv_tabview_get_tab_bar(tabview), 40);

    lv_obj_t * t1 = lv_tabview_add_tab(tabview, "GPS Data");
    lv_obj_t * t2 = lv_tabview_add_tab(tabview, "Air Qual");
    lv_obj_t * t3 = lv_tabview_add_tab(tabview, "System");

    // Top Bar Elements
    label_wifi_icon = lv_label_create(lv_layer_top());
    lv_label_set_text(label_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_align(label_wifi_icon, LV_ALIGN_TOP_RIGHT, -10, 5);
    lv_obj_set_style_text_color(label_wifi_icon, lv_color_hex(0x555555), 0); // Gray init

    label_status = lv_label_create(lv_layer_top());
    lv_label_set_text(label_status, "System Init...");
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(label_status, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(label_status, LV_OPA_50, 0);

    // Tab 1: GPS
    label_gps = lv_label_create(t1);
    lv_label_set_text(label_gps, "GPS Waiting...");
    lv_obj_center(label_gps);
    
    // Tab 2: Air
    label_air = lv_label_create(t2);
    lv_label_set_text(label_air, "Sensor Init...");
    lv_obj_center(label_air);
    
    // Tab 3: System
    lv_obj_t * lbl = lv_label_create(t3);
    lv_label_set_text(lbl, "ESP32 CYD Tracker\nEngineering Project\n\n(c) 2024");
    lv_obj_center(lbl);
}

// ==========================================
// 5. SETUP
// ==========================================
void setup() {
    // CRITICAL: Disable brownout detector to prevent reboots on USB power
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    gpsSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);

    tft.begin();
    tft.setRotation(1);
    tft.setSwapBytes(true);

    touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
    touchscreen.begin(touchSPI);
    touchscreen.setRotation(1);

    lv_init();
    lv_display_t * disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    create_gui();
    startupTimer = millis();
}

// ==========================================
// 6. LOOP
// ==========================================
void loop() {
    lv_task_handler(); 
    lv_tick_inc(5); 

    // --- 1. SAFE STARTUP (Delay WiFi by 4s) ---
    if (!wifiStarted && millis() - startupTimer > 4000) {
        wifiStarted = true;
        lv_label_set_text(label_status, "WiFi Connecting...");
        
        WiFi.mode(WIFI_STA);
        // Reduced TX Power to avoid voltage drops
        WiFi.setTxPower(WIFI_POWER_11dBm); 
        WiFi.begin(ssid, password);
    }

    // --- 2. WIFI MANAGER ---
    if (wifiStarted) {
        static unsigned long lastCheck = 0;
        if (millis() - lastCheck > 1000) {
            lastCheck = millis();
            
            if (WiFi.status() == WL_CONNECTED) {
                lv_obj_set_style_text_color(label_wifi_icon, lv_color_hex(0x00FF00), 0);
                
                if (!firebaseReady) {
                    config.api_key = API_KEY;
                    config.database_url = DATABASE_URL;
                    fbdo.setResponseSize(4096);
                    config.timeout.wifiReconnect = 10000;
                    
                    Firebase.signUp(&config, &auth, "", "");
                    Firebase.begin(&config, &auth);
                    Firebase.reconnectWiFi(true);
                    firebaseReady = true;
                    lv_label_set_text(label_status, "System ONLINE");
                }
            } else {
                 lv_obj_set_style_text_color(label_wifi_icon, lv_color_hex(0xFF0000), 0);
            }
        }
    }

    // --- 3. GPS READ ---
    while (gpsSerial.available() > 0) {
        gps.encode(gpsSerial.read());
    }

    // --- 4. MAIN LOGIC (Every 5s) ---
    if (millis() - lastSend > 5000) {
        lastSend = millis();

        // Simulated Air Data
        airQuality += random(-5, 6);
        if(airQuality < 0) airQuality = 0;
        
        float lat = 0.0;
        float lng = 0.0;
        int sats = 0;
        bool validGPS = false;

        // GPS Check
        if (gps.location.isValid()) {
            lat = gps.location.lat();
            lng = gps.location.lng();
            sats = gps.satellites.value();
            validGPS = true;
            
            // Safe String Concatenation (fixes compilation errors)
            String g_txt = "Lat: ";
            g_txt += String(lat, 6);
            g_txt += "\nLon: ";
            g_txt += String(lng, 6);
            g_txt += "\nSats: ";
            g_txt += sats;
            
            lv_label_set_text(label_gps, g_txt.c_str());
        } else {
             lv_label_set_text(label_gps, "Searching Satellites...\n(Go outside)");
        }

        // Air Data Display
        String a_txt = "PM 2.5: ";
        a_txt += airQuality;
        a_txt += " ug/m3\nStatus: ";
        if(airQuality < 20) {
            a_txt += "Good";
            lv_obj_set_style_text_color(label_air, lv_color_hex(0x00FF00), 0); 
        } else {
            a_txt += "Moderate";
            lv_obj_set_style_text_color(label_air, lv_color_hex(0xFFFF00), 0); 
        }
        lv_label_set_text(label_air, a_txt.c_str());

        // Send to Cloud (Only if GPS Fix is Valid)
        if (firebaseReady && validGPS) {
            lv_label_set_text(label_status, "Sending data...");
            
            FirebaseJson json;
            json.set("lat", lat);
            json.set("lng", lng);
            json.set("air", airQuality);
            json.set("ts", millis());

            if (Firebase.RTDB.pushJSON(&fbdo, "/routes", &json)) {
                lv_label_set_text(label_status, "Saved to Cloud");
            } else {
                lv_label_set_text(label_status, "DB Error");
            }
        }
    }
    
    delay(5);
}