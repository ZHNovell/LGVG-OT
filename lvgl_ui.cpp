#include <lvgl.h>
#include <TFT_eSPI.h>
#include <OTGW.h>   // существующий объект OTGateway из проекта

TFT_eSPI tft = TFT_eSPI();
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];

extern OTGW otGateway; // использовать глобальный объект OTGateway из проекта

// === Глобальные элементы ===
lv_obj_t *ch_page, *dhw_page, *diag_page;
lv_obj_t *tab_ch_btn, *tab_dhw_btn, *tab_diag_btn;

// CH
lv_obj_t *ch_setpoint_arc, *ch_temperature_label, *ch_action_label;
static lv_obj_t *ch_chart;
static lv_chart_series_t *ser_ch_temp, *ser_ch_setpoint;

// DHW
lv_obj_t *dhw_temp_label, *dhw_set_label;
static lv_obj_t *dhw_chart;
static lv_chart_series_t *ser_dhw_temp, *ser_dhw_set;

// Diagnostics
lv_obj_t *pressure_label, *error_label, *flame_label, *service_label;
lv_obj_t *burner_bar, *burner_label;

// === Страницы ===
void show_page(lv_obj_t *page) {
    lv_obj_add_flag(ch_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(dhw_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(diag_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_HIDDEN);
}

// === Flush дисплея для ST7701S ===
void my_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// === Тачскрин GT911 ===
#include "gt911.h"
static GT911 touch(GT911_INT_PIN, GT911_RST_PIN);

bool my_touch_read_cb(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    if (touch.available()) {
        int16_t x, y;
        if (touch.read(&x, &y)) {
            data->point.x = x;
            data->point.y = y;
            data->state = LV_INDEV_STATE_PRESSED;
            return false;
        }
    }
    data->state = LV_INDEV_STATE_RELEASED;
    return false;
}

// === Верхнее меню ===
void ui_add_top_menu() {
    tab_ch_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(tab_ch_btn, 80, 40);
    lv_obj_align(tab_ch_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_t *l1 = lv_label_create(tab_ch_btn);
    lv_label_set_text(l1, "CH");
    lv_obj_center(l1);
    lv_obj_add_event_cb(tab_ch_btn, [](lv_event_t * e){ show_page(ch_page); }, LV_EVENT_CLICKED, NULL);

    tab_dhw_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(tab_dhw_btn, 80, 40);
    lv_obj_align(tab_dhw_btn, LV_ALIGN_TOP_LEFT, 100, 10);
    lv_obj_t *l2 = lv_label_create(tab_dhw_btn);
    lv_label_set_text(l2, "DHW");
    lv_obj_center(l2);
    lv_obj_add_event_cb(tab_dhw_btn, [](lv_event_t * e){ show_page(dhw_page); }, LV_EVENT_CLICKED, NULL);

    tab_diag_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(tab_diag_btn, 120, 40);
    lv_obj_align(tab_diag_btn, LV_ALIGN_TOP_LEFT, 190, 10);
    lv_obj_t *l3 = lv_label_create(tab_diag_btn);
    lv_label_set_text(l3, "Diagnostics");
    lv_obj_center(l3);
    lv_obj_add_event_cb(tab_diag_btn, [](lv_event_t * e){ show_page(diag_page); }, LV_EVENT_CLICKED, NULL);
}

// === UI CH ===
void ui_add_ch(lv_obj_t *parent) {
    ch_setpoint_arc = lv_arc_create(parent);
    lv_obj_set_size(ch_setpoint_arc, 300, 300);
    lv_obj_center(ch_setpoint_arc);
    lv_arc_set_range(ch_setpoint_arc, 180, 300);
    lv_arc_set_rotation(ch_setpoint_arc, 270);
    lv_obj_remove_style(ch_setpoint_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(ch_setpoint_arc, LV_OBJ_FLAG_CLICKABLE);

    ch_action_label = lv_label_create(parent);
    lv_label_set_text(ch_action_label, "--");
    lv_obj_align(ch_action_label, LV_ALIGN_CENTER, 0, -80);

    ch_temperature_label = lv_label_create(parent);
    lv_label_set_text(ch_temperature_label, "--.-°C");
    lv_obj_align(ch_temperature_label, LV_ALIGN_CENTER, 0, 100);

    ch_chart = lv_chart_create(parent);
    lv_obj_set_size(ch_chart, 400, 120);
    lv_obj_align(ch_chart, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_chart_set_range(ch_chart, LV_CHART_AXIS_PRIMARY_Y, 10, 90);
    lv_chart_set_point_count(ch_chart, 50);
    ser_ch_temp = lv_chart_add_series(ch_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    ser_ch_setpoint = lv_chart_add_series(ch_chart, lv_palette_main(LV_PALETTE_ORANGE), LV_CHART_AXIS_PRIMARY_Y);
}

// === UI DHW ===
void ui_add_dhw(lv_obj_t *parent) {
    dhw_temp_label = lv_label_create(parent);
    lv_label_set_text(dhw_temp_label, "--.-°C");
    lv_obj_align(dhw_temp_label, LV_ALIGN_CENTER, 0, -50);

    dhw_set_label = lv_label_create(parent);
    lv_label_set_text(dhw_set_label, "Set: --°C");
    lv_obj_align(dhw_set_label, LV_ALIGN_CENTER, 0, 20);

    dhw_chart = lv_chart_create(parent);
    lv_obj_set_size(dhw_chart, 400, 150);
    lv_obj_align(dhw_chart, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_chart_set_range(dhw_chart, LV_CHART_AXIS_PRIMARY_Y, 10, 80);
    lv_chart_set_point_count(dhw_chart, 50);
    ser_dhw_temp = lv_chart_add_series(dhw_chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    ser_dhw_set = lv_chart_add_series(dhw_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
}

// === UI Diagnostics ===
void ui_add_diag(lv_obj_t *parent) {
    pressure_label = lv_label_create(parent);
    lv_label_set_text(pressure_label, "Pressure: -- bar");
    lv_obj_align(pressure_label, LV_ALIGN_TOP_LEFT, 20, 20);

    error_label = lv_label_create(parent);
    lv_label_set_text(error_label, "Error: none");
    lv_obj_align(error_label, LV_ALIGN_TOP_LEFT, 20, 60);

    flame_label = lv_label_create(parent);
    lv_label_set_text(flame_label, "Flame: --");
    lv_obj_align(flame_label, LV_ALIGN_TOP_LEFT, 20, 100);

    service_label = lv_label_create(parent);
    lv_label_set_text(service_label, "Service: --");
    lv_obj_align(service_label, LV_ALIGN_TOP_LEFT, 20, 140);

    burner_bar = lv_bar_create(parent);
    lv_obj_set_size(burner_bar, 300, 20);
    lv_obj_align(burner_bar, LV_ALIGN_TOP_LEFT, 20, 200);
    lv_bar_set_range(burner_bar, 0, 100);
    lv_bar_set_value(burner_bar, 0, LV_ANIM_OFF);

    burner_label = lv_label_create(parent);
    lv_label_set_text(burner_label, "0%");
    lv_obj_align_to(burner_label, burner_bar, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
}

// === UI INIT ===
void ui_begin() {
    ui_add_top_menu();

    ch_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ch_page, 480, 480);
    lv_obj_align(ch_page, LV_ALIGN_BOTTOM_MID, 0, 0);
    ui_add_ch(ch_page);

    dhw_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dhw_page, 480, 480);
    lv_obj_add_flag(dhw_page, LV_OBJ_FLAG_HIDDEN);
    ui_add_dhw(dhw_page);

    diag_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(diag_page, 480, 480);
    lv_obj_add_flag(diag_page, LV_OBJ_FLAG_HIDDEN);
    ui_add_diag(diag_page);

    show_page(ch_page);
}

// === UI Update ===
void ui_update() {
    // CH
    float ch_temp = otGateway.getBoilerTemperature();
    float ch_set = otGateway.getCHSetpoint();
    lv_label_set_text_fmt(ch_temperature_label, "%.1f°C", ch_temp);
    lv_arc_set_value(ch_setpoint_arc, ch_set * 10);
    lv_chart_set_next_value(ch_chart, ser_ch_temp, ch_temp);
    lv_chart_set_next_value(ch_chart, ser_ch_setpoint, ch_set);

    // DHW
    float dhw_temp = otGateway.getDHWTemperature();
    float dhw_set = otGateway.getDHWSetpoint();
    lv_label_set_text_fmt(dhw_temp_label, "%.1f°C", dhw_temp);
    lv_label_set_text_fmt(dhw_set_label, "Set: %.1f°C", dhw_set);
    lv_chart_set_next_value(dhw_chart, ser_dhw_temp, dhw_temp);
    lv_chart_set_next_value(dhw_chart, ser_dhw_set, dhw_set);

    // Diagnostics
    lv_label_set_text_fmt(pressure_label, "Pressure: %.1f bar", otGateway.getPressure());
    lv_label_set_text_fmt(error_label, "Error: %s", otGateway.getError().c_str());
    lv_label_set_text_fmt(flame_label, "Flame: %s", otGateway.getFlameStatus() ? "ON" : "OFF");
    lv_label_set_text_fmt(service_label, "Service: %s", otGateway.getServiceRequest() ? "YES" : "NO");

    int mod = otGateway.getBurnerModulation();
    lv_bar_set_value(burner_bar, mod, LV_ANIM_OFF);
    lv_label_set_text_fmt(burner_label, "%d%%", mod);
}

// === Loop handler ===
void ui_loop() {
    lv_task_handler();
    delay(5);

    static unsigned long last = 0;
    if (millis() - last > 1000) {
        last = millis();
        otGateway.update();
        ui_update();
    }
}

// === Setup ===
void ui_setup() {
    lv_init();
    tft.begin();
    tft.setRotation(3);

    lv_disp_draw_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 480;
    disp_drv.flush_cb = my_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    otGateway.begin();
    touch.begin();

    ui_begin();
}
