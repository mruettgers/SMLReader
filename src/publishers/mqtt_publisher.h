#ifndef mqtt_publisher_h
#define mqtt_publisher_h

#include "config.h"
#include "debug.h"
#include "MQTT.h"

struct MqttConfig
{
  char server[128] = "mosquitto";
  char port[8] = "1883";
  char username[128];
  char password[128];
  char topic[128] = "iot/smartmeter/";
};

class MqttPublisher
{
public:
  void setup(MqttConfig _config)
  {
    DEBUG("Setting up MQTT publisher.");
    config = _config;
  }
  void connect()
  {
    DEBUG("Establishing MQTT client connection.");
  }
  void loop()
  {
  }
  void publish(metric_value *values)
  {
    // Publish
    int32_t value;
    for (uint8_t i = 0; i < NUM_OF_METRICS; i++)
    {
      //TODO:
      //sent to /topic/metric/[name]/value
      //sent to /topic/metric/[name]/json // with meta info
      value = (uint32_t)((values[i].value * (pow(10, values[i].scaler))) * 1000);
      DEBUG("Published metric '%s':", METRICS[i].name);
      DEBUG("  Value: %ld", (long)values[i].value);
      DEBUG("  Unit: %d", (int)values[i].unit);
      DEBUG("  Scaler: %d", (int)values[i].scaler);
    }
  }

private:
  MqttConfig config;
  bool connected = false;
};

#endif