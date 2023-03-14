#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"
#include "Sensor.h"

const char *VERSION = "2.3.0";

// Modifying the config version will probably cause a loss of the existig configuration.
// Be careful!
const char *CONFIG_VERSION = "2.0.0";

const char *WIFI_AP_SSID = "SMLReader";
const char *WIFI_AP_DEFAULT_PASSWORD = "";

#endif