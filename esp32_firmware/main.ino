#include <Arduino.h>
#include "driver/twai.h"
#include "CAN_OTA.h" 

const uint16_t TCS_OTA_ID = 0x267;
CanOtaManager CanOTA(TCS_OTA_ID);

#define TX_PIN GPIO_NUM_6 
#define RX_PIN GPIO_NUM_5 

void receiveCANData() {
    twai_message_t rx_msg;
    while (twai_receive(&rx_msg, 0) == ESP_OK) { 
        if (CanOTA.processMessage(rx_msg)) continue; 
        // Normal processing logic...
    }
}

void setup() {
    Serial.begin(115200);
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 25; 
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        twai_start();
    }
}

void loop() {
    receiveCANData();
    if (CanOTA.isInProgress()) {
        delay(1); 
        return; 
    }
    // Main application code...
}
