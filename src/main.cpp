#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include <time.h>              
#include <esp_task_wdt.h>      
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ==========================================
// 1. CONFIGURATION
// ==========================================
const char* ssid     = "*********";    
const char* password = "*********"; 
String thingSpeakApiKey = "******"; 

// Location: GDANSK
float fixedLat = 54.3520; 
float fixedLng = 18.6466; 

// Watchdog Timeout (seconds)
#define WDT_TIMEOUT 30

// ==========================================
// 2. HARDWARE
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

lv_obj_t * lbl_clock;       
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

// Reference to Air Tab for background color changes
lv_obj_t * tab_air; 

float valTemp = 0.0;
int valPM25 = 0;
unsigned long lastUpdateTimestamp = 0;
unsigned long updateInterval = 60000; 

// ==========================================
// 4. HELPER FUNCTIONS
// ==========================================
void setLedColor(bool r, bool g, bool b) {
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

// NTP Time Configuration
void initTime() {
    configTime(3600, 3600, "pool.ntp.org", "time.nist.gov"); // GMT+1 + DST
}

String getLocalTime() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        return "--:--";
    }
    char timeStringBuff[10];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
    return String(timeStringBuff);
}

// ==========================================
// 5. GUI SETUP
// ==========================================
void create_gui() {
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101010), 0);
    lv_obj_t * tabview = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);

    lv_obj_t * t1 = lv_tabview_add_tab(tabview, "SYSTEM");
    lv_obj_t * t2 = lv_tabview_add_tab(tabview, "AIR QUAL");
    lv_obj_t * t3 = lv_tabview_add_tab(tabview, "WEATHER");
    
    tab_air = t2; 

    // --- SYSTEM ---
    lbl_clock = lv_label_create(t1);
    lv_label_set_text(lbl_clock, "00:00");
    lv_obj_align(lbl_clock, LV_ALIGN_TOP_RIGHT, -10, 5);
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_clock, lv_color_hex(0x00FFFF), 0); // Cyan

    lbl_status_header = lv_label_create(t1);
    lv_label_set_text(lbl_status_header, "WiFi: ... | Gdansk");
    lv_obj_align(lbl_status_header, LV_ALIGN_TOP_LEFT, 10, 5);

    lv_obj_t * panel_gps = lv_obj_create(t1);
    lv_obj_set_size(panel_gps, 280, 90);
    lv_obj_align(panel_gps, LV_ALIGN_TOP_MID, 0, 40);
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
    lv_label_set_text(lbl_info_mode, "Loc Source: HARDCODED\nAuto-Update: Every 60s\nNTP Clock: Enabled");
    lv_obj_align(lbl_info_mode, LV_ALIGN_TOP_LEFT, 10, 140);
    lv_obj_set_style_text_color(lbl_info_mode, lv_palette_main(LV_PALETTE_GREY), 0);

    // --- AIR QUALITY (With Units) ---
    // Added "ug/m3" for scientific accuracy
    lbl_pm25 = lv_label_create(t2); lv_label_set_text(lbl_pm25, "PM 2.5: -- ug/m3"); lv_obj_align(lbl_pm25, LV_ALIGN_TOP_LEFT, 10, 20);
    lbl_pm10 = lv_label_create(t2); lv_label_set_text(lbl_pm10, "PM 10:  -- ug/m3"); lv_obj_align(lbl_pm10, LV_ALIGN_TOP_LEFT, 10, 50);
    
    // Gases
    lbl_no2  = lv_label_create(t2); lv_label_set_text(lbl_no2,  "NO2: -- ug/m3");    lv_obj_align(lbl_no2, LV_ALIGN_TOP_RIGHT, -10, 20);
    lbl_o3   = lv_label_create(t2); lv_label_set_text(lbl_o3,   "O3:  -- ug/m3");    lv_obj_align(lbl_o3, LV_ALIGN_TOP_RIGHT, -10, 45);
    lbl_so2  = lv_label_create(t2); lv_label_set_text(lbl_so2,  "SO2: -- ug/m3");    lv_obj_align(lbl_so2, LV_ALIGN_TOP_RIGHT, -10, 70);
    lbl_co   = lv_label_create(t2); lv_label_set_text(lbl_co,   "CO:  -- ug/m3");    lv_obj_align(lbl_co, LV_ALIGN_TOP_RIGHT, -10, 95);
    
    bar_summary = lv_bar_create(t2); 
    lv_obj_set_size(bar_summary, 260, 15); 
    lv_obj_align(bar_summary, LV_ALIGN_BOTTOM_MID, 0, -10);

    // --- WEATHER ---
    lbl_temp_big = lv_label_create(t3); lv_label_set_text(lbl_temp_big, "-- C"); lv_obj_center(lbl_temp_big);
    lv_obj_set_style_text_font(lbl_temp_big, &lv_font_montserrat_48, 0);
    lbl_press_val = lv_label_create(t3); lv_label_set_text(lbl_press_val, "Pressure: -- hPa"); lv_obj_align(lbl_press_val, LV_ALIGN_BOTTOM_MID, 0, -30);
}

// ==========================================
// 6. DATA SYNC LOGIC
// ==========================================
void syncData() {
    if(WiFi.status() != WL_CONNECTED) {
        setLedColor(true, false, false);
        lv_label_set_text(lbl_status_header, "WiFi: ERROR");
        return;
    }

    setLedColor(false, false, true); // Blue - Syncing
    HTTPClient http;
    JsonDocument doc; 

    // 1. WEATHER REQUEST
    String urlWeather = "http://api.open-meteo.com/v1/forecast?latitude=" + String(fixedLat, 4) + 
                        "&longitude=" + String(fixedLng, 4) + 
                        "&current=temperature_2m,surface_pressure";

    http.begin(urlWeather);
    if (http.GET() == 200) {
        deserializeJson(doc, http.getString());
        float temp  = doc["current"]["temperature_2m"];
        float press = doc["current"]["surface_pressure"];
        valTemp = temp;
        
        lv_label_set_text(lbl_temp_big, (String(temp, 1) + " C").c_str());
        lv_label_set_text(lbl_press_val, ("Pressure: " + String(press, 0) + " hPa").c_str());

        // Dynamic Temperature Color
        if(temp < 0) lv_obj_set_style_text_color(lbl_temp_big, lv_color_hex(0x3399FF), 0); // Cold = Blue
        else if(temp > 25) lv_obj_set_style_text_color(lbl_temp_big, lv_color_hex(0xFF3333), 0); // Hot = Red
        else lv_obj_set_style_text_color(lbl_temp_big, lv_color_white(), 0); // Normal = White
    }
    http.end();
    doc.clear();

    // 2. AIR QUALITY REQUEST
    String urlAir = "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + String(fixedLat, 4) + 
                    "&longitude=" + String(fixedLng, 4) + 
                    "&current=pm10,pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone";

    http.begin(urlAir);
    if (http.GET() == 200) {
        deserializeJson(doc, http.getString());
        float pm25 = doc["current"]["pm2_5"];
        float pm10 = doc["current"]["pm10"];
        float no2  = doc["current"]["nitrogen_dioxide"];
        float so2  = doc["current"]["sulphur_dioxide"];
        float o3   = doc["current"]["ozone"];
        float co   = doc["current"]["carbon_monoxide"];

        valPM25 = (int)pm25;

        // Update UI with Units
        lv_label_set_text(lbl_pm25, ("PM 2.5: " + String(pm25, 0) + " ug/m3").c_str());
        lv_label_set_text(lbl_pm10, ("PM 10:  " + String(pm10, 0) + " ug/m3").c_str());
        lv_label_set_text(lbl_no2,  ("NO2: " + String(no2, 1) + " ug/m3").c_str());
        lv_label_set_text(lbl_so2,  ("SO2: " + String(so2, 1) + " ug/m3").c_str());
        lv_label_set_text(lbl_o3,   ("O3:  " + String(o3, 1) + " ug/m3").c_str());
        lv_label_set_text(lbl_co,   ("CO:  " + String(co, 1) + " ug/m3").c_str());

        // Bar & Alarm Logic
        lv_bar_set_value(bar_summary, (int)pm25, LV_ANIM_ON);
        if(pm25 < 25) {
            lv_obj_set_style_bg_color(bar_summary, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(tab_air, lv_color_hex(0x101010), 0); // Normal BG
        } else if (pm25 < 50) {
            lv_obj_set_style_bg_color(bar_summary, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(tab_air, lv_color_hex(0x202000), 0); // Yellowish BG
        } else {
            lv_obj_set_style_bg_color(bar_summary, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(tab_air, lv_color_hex(0x300000), 0); // Red BG (ALARM)
        }
    }
    http.end();

    // 3. UPLOAD TO THINGSPEAK
    String tsUrl = "http://api.thingspeak.com/update?api_key=" + thingSpeakApiKey + 
                   "&field1=" + String(fixedLat, 5) + 
                   "&field2=" + String(fixedLng, 5) + 
                   "&field3=" + String(valPM25) +
                   "&field4=" + String(valTemp);
    http.begin(tsUrl);
    http.GET();
    http.end();

    setLedColor(false, true, false); // Green - Done
    lastUpdateTimestamp = millis();
    lv_label_set_text(lbl_status_header, "WiFi: OK | Gdansk");
    
    // Reset Watchdog
    esp_task_wdt_reset();
}

// ==========================================
// 7. SETUP & LOOP
// ==========================================
hw_timer_t * lvgl_timer = NULL;
void IRAM_ATTR onTimer() { lv_tick_inc(5); }

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
    Serial.begin(115200);

    // Watchdog Init
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);

    pinMode(CYD_LED_RED, OUTPUT); pinMode(CYD_LED_GREEN, OUTPUT); pinMode(CYD_LED_BLUE, OUTPUT);
    setLedColor(true, false, false);

    tft.begin(); tft.setRotation(1); tft.setSwapBytes(true);
    touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS); touchscreen.begin(touchSPI); touchscreen.setRotation(1);

    lv_init();
    lv_display_t * disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    lvgl_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(lvgl_timer, &onTimer, true);
    timerAlarmWrite(lvgl_timer, 5000, true);
    timerAlarmEnable(lvgl_timer);

    create_gui();

    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    WiFi.begin(ssid, password);
    
    // Fetch time on start
    initTime();
}

void loop() {
    lv_task_handler();
    esp_task_wdt_reset();

    // First sync
    if (WiFi.status() == WL_CONNECTED && lastUpdateTimestamp == 0) {
        syncData();
        initTime(); 
    }

    // Cyclic sync
    if (millis() - lastUpdateTimestamp > updateInterval) {
        syncData();
    }
    
    // Clock Update
    static unsigned long lastClockUpdate = 0;
    if(millis() - lastClockUpdate > 1000) {
        lastClockUpdate = millis();
        lv_label_set_text(lbl_clock, getLocalTime().c_str());
    }
    
    delay(5);
}