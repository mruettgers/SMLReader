#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"
#include "Sensor.h"

const char *VERSION = "2.1.2";

// Modifying the config version will probably cause a loss of the existig configuration.
// Be careful!
const char *CONFIG_VERSION = "1.0.2";
const uint8_t STATUS_PIN = LED_BUILTIN;

static const SensorConfig SENSOR_CONFIGS[] = {
    {.pin = D2,
     .name = "1",
     .numeric_only = false}};

const uint8_t NUM_OF_SENSORS = sizeof(SENSOR_CONFIGS) / sizeof(SensorConfig);

#endif