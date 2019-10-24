#ifndef CONFIG_H
#define CONFIG_H

#include <types.h>

const char* VERSION = "1.0.0";

// Modifying the config version will probably cause a loss of the existig configuration.
// Be careful!
const char* CONFIG_VERSION = "1.0.2";

const uint8_t SENSOR_PIN = 4;
const uint8_t ONEWIRE_PIN = 0;
const uint8_t STATUS_PIN = LED_BUILTIN;

// EHM ED300L
static const metric METRICS[] = {
    {"power_in", {0x77, 0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF}},
    {"power_out", {0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x00, 0xFF}},
    {"power_current", {0x77, 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF}}};




const uint8_t NUM_OF_METRICS = sizeof(METRICS) / sizeof(metric);
#endif