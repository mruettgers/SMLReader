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
    uint8_t lastCharOfTopic = strlen(config.topic) - 1;
    baseTopic = String(config.topic) + (lastCharOfTopic >= 0 && config.topic[lastCharOfTopic] == '/' ? "" : "/");
  }
  void connect()
  {
    DEBUG("Establishing MQTT client connection.");
    client.connect("SMLReader");
    if (client.connected()) {
      char message[32];
      snprintf(message,32,"Hello from %08X.",ESP.getChipId());
      info(message);
    }
  }

  void loop()
  {
    if (client)
    {
      client.loop();
    }
  }

  void publish(metric_value *values)
  {

    if (!client.connected())
    {
      connect();
    }

    if (!client.connected())
    {
      // Something failed
      DEBUG("Connection to MQTT broker could not be established. Omitted publishing of metrics.");
      return;
    }

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
  MQTTClient client;
  bool connected = false;
  String baseTopic;

  void info(char *message)
  {
    String infoTopic = baseTopic + "info";
    DEBUG(infoTopic.c_str());

    for (uint8_t i = 0; i < NUM_OF_METRICS; i++)
    {
      String metricTopic = baseTopic + "metric/" + METRICS[i].name + "/";
      DEBUG(metricTopic.c_str());
    }
  }
};

#endif