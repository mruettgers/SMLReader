#ifndef CONFIG_H
#define CONFIG_H

#include <types.h>

const char* VERSION = "2.0.0";

// Modifying the config version will probably cause a loss of the existig configuration.
// Be careful!
const char* CONFIG_VERSION = "1.0.2";
const uint8_t STATUS_PIN = LED_BUILTIN;

const uint8_t SENSOR_PIN = 4;

#endif