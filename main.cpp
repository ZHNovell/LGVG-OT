#include <Arduino.h>
#include "lvgl_ui.h"   // наш LVGL UI
#include <OTGW.h>

// Глобальный объект OTGateway из проекта
OTGW otGateway;

void setup() {
    Serial.begin(115200);
    delay(100);

    // Инициализация LVGL и дисплея/тача
    ui_setup();

    Serial.println("OTGateway + LVGL UI initialized");
}

void loop() {
    // Обработка LVGL и обновление интерфейса
    ui_loop();

    // Можно добавить другие задачи OTGateway, если нужно
    // otGateway.update(); // уже вызывается внутри ui_loop()
}
