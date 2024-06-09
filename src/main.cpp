#include "Audio.h"
#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <lvgl.h>

#include "ui.h"

#define I2S_DOUT 25
#define I2S_BCLK 26
#define I2S_LRC 27

static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

Preferences preferences;

Audio audio;

String ssid = "****";
String password = "****";

String stations[] = {
    "0n-80s.radionetz.de:8000/0n-70s.mp3",
    "https://music.163.com/song/media/outer/url?id=1932354158",
    "www.surfmusic.de/m3u/100-5-das-hitradio,4529.m3u",
    "stream.1a-webradio.de/deutsch/mp3-128/vtuner-1a",
    "mp3.ffh.de/radioffh/hqlivestream.aac", //  128k aac
    "www.antenne.de/webradio/antenne.m3u",
    "listen.rusongs.ru/ru-mp3-128",
    "edge.audio.3qsdn.com/senderkw-mp3",
    "macslons-irish-pub-radio.com/media.asx",
};

uint8_t max_volume = 21;
uint8_t max_stations = 0;
uint8_t cur_station = 3;
uint8_t cur_volume = 0;
int8_t cur_btn = -1;

void audioPlay();

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint16_t touchX, touchY;

    bool touched = tft.getTouch(&touchX, &touchY, 600);

    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;

        data->point.x = touchX * 1.33;
        data->point.y = touchY * 0.75;
    }
}

String musicSubstring(String str)
{
    int lastSlashIndex = str.lastIndexOf('/');
    if (lastSlashIndex != -1) {
        return str.substring(lastSlashIndex + 1);
    }
    return "";
}

String optionsGet()
{
    String options;
    for (int i = 0; i < sizeof(stations) / sizeof(stations[0]); i++) {
        String url = stations[i];
        options += musicSubstring(url);
        options += "\n";
    }
    options.trim();
    return options;
}

void audioVolume(int volume)
{
    cur_volume = volume;
    preferences.putInt("volume", cur_volume);
    audio.setVolume(volume);
}

void audioStation(int station)
{
    cur_station = station;
    preferences.putInt("station", cur_station);
    audioPlay();
}
void audioPrevious()
{
    if (cur_station > 0) {
        cur_station--;
        audioStation(cur_station);
    }
}

void audioNext()
{
    if (cur_station < max_stations - 1) {
        cur_station++;
        audioStation(cur_station);
    }
}

void audioPlay()
{
    Serial.println("Play");
    lv_dropdown_set_selected(ui_musicDropdown, cur_station);
    lv_label_set_text(ui_Label25, musicSubstring(stations[cur_station]).c_str());
    if (audio.isRunning()) {
        audio.stopSong();
    }
    if (audio.connecttohost(stations[cur_station].c_str())) {
        Serial.println("Connect to host");
        lv_label_set_text(ui_playLabel, LV_SYMBOL_PAUSE);
    } else {
        Serial.println("Connect to host failed");
        lv_label_set_text(ui_playLabel, LV_SYMBOL_PLAY);
    }
    Serial.printf("cur station %s\n", stations[cur_station].c_str());
}

void audioPause()
{
    audio.stopSong();
    Serial.println(audio.isRunning());
}

void musicbtnCD(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn == ui_prevButton) {
        Serial.println("prev");
        audioPrevious();
    } else if (btn == ui_nextButton) {
        Serial.println("next");
        audioNext();
    } else if (btn == ui_playButton) {
        if (!audio.isRunning()) {
            audioPlay();
        } else {
            audioPause();
            lv_label_set_text(ui_playLabel, LV_SYMBOL_PLAY);
        }
    }
}

static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t volume = ((int)lv_slider_get_value(slider) * 21 / 100);
    Serial.println(volume);
    audioVolume(volume);
}

static void dropdown_event_cd(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        audioStation(lv_dropdown_get_selected(obj));
    }
}

void lvgl_task(void *pt)
{
    while (1) {
        lv_timer_handler();
        vTaskDelay(5);
    }
    vTaskDelete(NULL);
}

void audioTask(void *pt)
{
    while (1) {
        audio.loop();
        vTaskDelay(2);
    }
    vTaskDelete(NULL);
}

void setup()
{
    Serial.begin(115200);

    preferences.begin("settings", false);

    if (preferences.getInt("volume", 1000) == 1000) {
        preferences.putInt("volume", 10);
        preferences.putInt("station", 0);
    } else {
        cur_station = (int)preferences.getInt("station");
        Serial.println(cur_station);
        cur_volume = preferences.getInt("volume");
        Serial.println(cur_volume);
    }

    max_stations = sizeof(stations) / sizeof(stations[0]);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(cur_volume);

    lv_init();

    tft.begin();
    tft.setRotation(3);
    uint16_t calData[5] = {490, 3259, 422, 3210, 1};
    tft.setTouch(calData);

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /*Initialize the (dummy) input device driver*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();
    lv_label_set_text(ui_prevLabel, LV_SYMBOL_PREV);
    lv_label_set_text(ui_nextLabel, LV_SYMBOL_NEXT);
    lv_label_set_text(ui_playLabel, LV_SYMBOL_PLAY);

    lv_obj_add_event_cb(ui_prevButton, musicbtnCD, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_nextButton, musicbtnCD, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_playButton, musicbtnCD, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_volumeSlider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_dropdown_set_options(ui_musicDropdown, optionsGet().c_str());
    lv_obj_add_event_cb(ui_musicDropdown, dropdown_event_cd, LV_EVENT_ALL, NULL);

    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 1024 * 10, NULL, 2, NULL, 1);

    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
        delay(2000);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\r\n-- wifi connect success! --\r\n");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }

    xTaskCreatePinnedToCore(audioTask, "audio_task", 1024 * 5, NULL, 2, NULL, 1);
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.print("re connect");
        WiFi.begin(ssid.c_str(), password.c_str());
        while (WiFi.status() != WL_CONNECTED) {
            delay(2000);
            Serial.print(".");
        }
    }
    vTaskDelay(1000);
}