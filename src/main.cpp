#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ==========================================
// 1. CONFIGURATION
// ==========================================
const char* ssid     = "HALNy-2.4G-8228b4";    
const char* password = "H55Luw8HLw"; 
String thingSpeakApiKey = "EW9MCXI8KVWNMQFU"; 

// --- LOCATION: GDANSK (City Center) ---
float fixedLat = 54.3520; 
float fixedLng = 18.6466; 

// ==========================================
// 2. HARDWARE SETUP
// ==========================================
#define XPT_CS   33 
#define XPT_MOSI 32
#define XPT_MISO 39
#define XPT_CLK  25
#define CYD_LED_RED   4
#define CYD_LED_GREEN 16
#define CYD_LED_BLUE  17

XPT2046_Touchscreen touchscreen(XPT_CS); 
SPIClass touchSPI(HSPI);
TFT_eSPI tft = TFT_eSPI();

// ==========================================
// 3. UI VARIABLES
// ==========================================
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
uint32_t draw_buf[SCREEN_WIDTH * SCREEN_HEIGHT / 10];

lv_obj_t * lbl_status_header;
lv_obj_t * lbl_lat_val;
lv_obj_t * lbl_lng_val;
lv_obj_t * lbl_info_mode;

lv_obj_t * lbl_temp_big;
lv_obj_t * lbl_press_val;

lv_obj_t * lbl_pm25;
lv_obj_t * lbl_pm10;
lv_obj_t * lbl_no2;
lv_obj_t * lbl_so2;
lv_obj_t * lbl_o3;
lv_obj_t * lbl_co;
lv_obj_t * bar_summary;

float valTemp = 0.0;
int valPM25 = 0;
unsigned long lastUpdateTimestamp = 0;
unsigned long updateInterval = 60000; // Every 60 seconds

// ==========================================
// 4. HELPER FUNCTIONS
// ==========================================
void setLedColor(bool r, bool g, bool b) {
    // CYD LEDs are usually active LOW
    digitalWrite(CYD_LED_RED,   r ? LOW : HIGH);
    digitalWrite(CYD_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(CYD_LED_BLUE,  b ? LOW : HIGH);
}

void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    if(touchscreen.touched()) {
        TS_Point p = touchscreen.getPoint();
        int tx = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
        int ty = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
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
// 5. GUI SETUP (NO BUTTON)
// ==========================================
void create_gui() {
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101010), 0);
    lv_obj_t * tabview = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);

    lv_obj_t * t1 = lv_tabview_add_tab(tabview, "SYSTEM");
    lv_obj_t * t2 = lv_tabview_add_tab(tabview, "AIR QUAL");
    lv_obj_t * t3 = lv_tabview_add_tab(tabview, "WEATHER");

    // --- SYSTEM TAB ---
    lbl_status_header = lv_label_create(t1);
    lv_label_set_text(lbl_status_header, "WiFi: ... | Mode: STATIC (Gdansk)");
    lv_obj_align(lbl_status_header, LV_ALIGN_TOP_MID, 0, 5);

    lv_obj_t * panel_gps = lv_obj_create(t1);
    lv_obj_set_size(panel_gps, 280, 90);
    lv_obj_align(panel_gps, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_bg_color(panel_gps, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(panel_gps, lv_palette_main(LV_PALETTE_GREEN), 0); 
    
    lbl_lat_val = lv_label_create(panel_gps);
    lv_label_set_text(lbl_lat_val, ("LAT: " + String(fixedLat, 4)).c_str());
    lv_obj_align(lbl_lat_val, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_color(lbl_lat_val, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_lat_val, &lv_font_montserrat_20, 0);

    lbl_lng_val = lv_label_create(panel_gps);
    lv_label_set_text(lbl_lng_val, ("LNG: " + String(fixedLng, 4)).c_str());
    lv_obj_align(lbl_lng_val, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_color(lbl_lng_val, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_lng_val, &lv_font_montserrat_20, 0);

    lbl_info_mode = lv_label_create(t1);
    lv_label_set_text(lbl_info_mode, "Loc Source: HARDCODED\nAuto-Update: Every 60s");
    lv_obj_align(lbl_info_mode, LV_ALIGN_TOP_LEFT, 10, 140);
    lv_obj_set_style_text_color(lbl_info_mode, lv_palette_main(LV_PALETTE_GREY), 0);

    // --- AIR TAB ---
    lbl_pm25 = lv_label_create(t2); lv_label_set_text(lbl_pm25, "PM 2.5: --"); lv_obj_align(lbl_pm25, LV_ALIGN_TOP_LEFT, 10, 20);
    lbl_pm10 = lv_label_create(t2); lv_label_set_text(lbl_pm10, "PM 10:  --"); lv_obj_align(lbl_pm10, LV_ALIGN_TOP_LEFT, 10, 50);
    lbl_no2  = lv_label_create(t2); lv_label_set_text(lbl_no2,  "NO2: --");    lv_obj_align(lbl_no2, LV_ALIGN_TOP_RIGHT, -10, 20);
    lbl_o3   = lv_label_create(t2); lv_label_set_text(lbl_o3,   "O3:  --");    lv_obj_align(lbl_o3, LV_ALIGN_TOP_RIGHT, -10, 45);
    lbl_so2  = lv_label_create(t2); lv_label_set_text(lbl_so2,  "SO2: --");    lv_obj_align(lbl_so2, LV_ALIGN_TOP_RIGHT, -10, 70);
    lbl_co   = lv_label_create(t2); lv_label_set_text(lbl_co,   "CO:  --");    lv_obj_align(lbl_co, LV_ALIGN_TOP_RIGHT, -10, 95);
    
    bar_summary = lv_bar_create(t2); 
    lv_obj_set_size(bar_summary, 260, 15); 
    lv_obj_align(bar_summary, LV_ALIGN_BOTTOM_MID, 0, -10);

    // --- WEATHER TAB ---
    lbl_temp_big = lv_label_create(t3); lv_label_set_text(lbl_temp_big, "-- C"); lv_obj_center(lbl_temp_big);
    lv_obj_set_style_text_font(lbl_temp_big, &lv_font_montserrat_48, 0);
    lbl_press_val = lv_label_create(t3); lv_label_set_text(lbl_press_val, "Pressure: --"); lv_obj_align(lbl_press_val, LV_ALIGN_BOTTOM_MID, 0, -30);
}

// ==========================================
// 6. DATA FETCHING LOGIC (ZERO VALUE FIX)
// ==========================================
void syncData() {
    if(WiFi.status() != WL_CONNECTED) {
        setLedColor(true, false, false); // RED - Error
        lv_label_set_text(lbl_status_header, "WiFi: ERROR");
        return;
    }

    setLedColor(false, false, true); // BLUE - Working
    HTTPClient http;
    JsonDocument doc; 

    

    // -----------------------------------------
    // STEP 1: FETCH WEATHER (api.open-meteo.com)
    // -----------------------------------------
    String urlWeather = "http://api.open-meteo.com/v1/forecast?latitude=" + String(fixedLat, 4) + 
                        "&longitude=" + String(fixedLng, 4) + 
                        "&current=temperature_2m,surface_pressure";

    http.begin(urlWeather);
    if (http.GET() == 200) {
        deserializeJson(doc, http.getString());
        float temp  = doc["current"]["temperature_2m"];
        float press = doc["current"]["surface_pressure"];
        
        valTemp = temp; // Save globally
        
        // Update Weather UI
        lv_label_set_text(lbl_temp_big, (String(temp, 1) + " C").c_str());
        lv_label_set_text(lbl_press_val, ("Pressure: " + String(press, 0) + " hPa").c_str());
    }
    http.end();
    
    doc.clear(); // Clear buffer

    // -----------------------------------------
    // STEP 2: FETCH AIR QUALITY (air-quality-api.open-meteo.com)
    // Crucial: Dedicated server for pollutants!
    // -----------------------------------------
    String urlAir = "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + String(fixedLat, 4) + 
                    "&longitude=" + String(fixedLng, 4) + 
                    "&current=pm10,pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone";

    http.begin(urlAir);
    if (http.GET() == 200) {
        deserializeJson(doc, http.getString());
        
        // Read as float to avoid data loss
        float pm25 = doc["current"]["pm2_5"];
        float pm10 = doc["current"]["pm10"];
        float no2  = doc["current"]["nitrogen_dioxide"];
        float so2  = doc["current"]["sulphur_dioxide"];
        float o3   = doc["current"]["ozone"];
        float co   = doc["current"]["carbon_monoxide"];

        valPM25 = (int)pm25; // Save globally

        // Update Air UI
        lv_label_set_text(lbl_pm25, ("PM 2.5: " + String(pm25, 0)).c_str());
        lv_label_set_text(lbl_pm10, ("PM 10:  " + String(pm10, 0)).c_str());
        lv_label_set_text(lbl_no2,  ("NO2: " + String(no2, 1)).c_str());
        lv_label_set_text(lbl_so2,  ("SO2: " + String(so2, 1)).c_str());
        lv_label_set_text(lbl_o3,   ("O3:  " + String(o3, 1)).c_str());
        lv_label_set_text(lbl_co,   ("CO:  " + String(co, 1)).c_str());

        // Bar Color Logic
        lv_bar_set_value(bar_summary, (int)pm25, LV_ANIM_ON);
        if(pm25 < 25) lv_obj_set_style_bg_color(bar_summary, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
        else lv_obj_set_style_bg_color(bar_summary, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    }
    http.end();

    // -----------------------------------------
    // STEP 3: UPLOAD TO THINGSPEAK
    // -----------------------------------------
    String tsUrl = "http://api.thingspeak.com/update?api_key=" + thingSpeakApiKey + 
                   "&field1=" + String(fixedLat, 5) + 
                   "&field2=" + String(fixedLng, 5) + 
                   "&field3=" + String(valPM25) +
                   "&field4=" + String(valTemp);
    http.begin(tsUrl);
    http.GET();
    http.end();

    setLedColor(false, true, false); // GREEN - Success
    lastUpdateTimestamp = millis();
    lv_label_set_text(lbl_status_header, "WiFi: OK | Gdansk (Updated)");
}

// ==========================================
// 7. SETUP & LOOP
// ==========================================
hw_timer_t * lvgl_timer = NULL;
void IRAM_ATTR onTimer() { lv_tick_inc(5); }

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
    Serial.begin(115200);

    // LED Setup
    pinMode(CYD_LED_RED, OUTPUT); pinMode(CYD_LED_GREEN, OUTPUT); pinMode(CYD_LED_BLUE, OUTPUT);
    setLedColor(true, false, false); // Start RED

    // Screen Setup
    tft.begin(); tft.setRotation(1); tft.setSwapBytes(true);
    touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS); touchscreen.begin(touchSPI); touchscreen.setRotation(1);

    // LVGL Setup
    lv_init();
    lv_display_t * disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // Timer Interrupt
    lvgl_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(lvgl_timer, &onTimer, true);
    timerAlarmWrite(lvgl_timer, 5000, true);
    timerAlarmEnable(lvgl_timer);

    create_gui();

    // WiFi Start
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    WiFi.begin(ssid, password);
}

void loop() {
    lv_task_handler();

    // Sync on first connection
    if (WiFi.status() == WL_CONNECTED && lastUpdateTimestamp == 0) {
        syncData();
    }

    // Timer Sync
    if (millis() - lastUpdateTimestamp > updateInterval) {
        syncData();
    }
    
    delay(5);
}