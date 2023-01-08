#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"
#include "Sensor.h"

const char *VERSION = "3.0.0";

// Modifying the config version will probably cause a loss of the existig configuration.
// Be careful!
const char *CONFIG_VERSION = "1.0.2";

const char *WIFI_AP_SSID = "SMLReader";
const char *WIFI_AP_DEFAULT_PASSWORD = "";

#define ESP32_ETH

static const SensorConfig SENSOR_CONFIGS[] = {
    {.uart = &Serial1,
     .pin = 2,
     .name = "1",
     .numeric_only = false,
     .status_led_enabled = false,
     .status_led_inverted = true,
     .status_led_pin = 0,
     .interval = 0},

    {.uart = nullptr,
     .pin = 36,
     .name = "2",
     .numeric_only = false,
     .status_led_enabled = false,
     .status_led_inverted = true,
     .status_led_pin = 0,
     .interval = 0},

     {.uart = &Serial2,
     .pin = 16,
     .name = "3",
     .numeric_only = false,
     .status_led_enabled = false,
     .status_led_inverted = true,
     .status_led_pin = 0,
     .interval = 0}

};

const uint8_t NUM_OF_SENSORS = sizeof(SENSOR_CONFIGS) / sizeof(SensorConfig);

#endif