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

    client.begin(config.server, atoi(config.port), net);
  }

  void connect()
  {
    DEBUG("Establishing MQTT client connection.");
    client.connect("SMLReader", config.username, config.password);
    if (client.connected())
    {
      char message[64];
      snprintf(message, 64, "Hello from %08X, running SMLReader version %s.", ESP.getChipId(), VERSION);
      info(message);
    }
  }

  void loop()
  {
    client.loop();
  }

  void debug(const char *message)
  {
    publish(baseTopic + "debug", message);
  }

  void info(const char *message)
  {
    publish(baseTopic + "info", message);
  }

  void publish(const metric_value *values)
  {
    // Publish
    int64_t value;
    char buf[21];

    for (uint8_t i = 0; i < NUM_OF_METRICS; i++)
    {

      String metricTopic = baseTopic + "metric/" + METRICS[i].name + "/";

      value = ((values[i].value * (pow(10, values[i].scaler))) * 1000);

      sprintf(buf,"%lld", value);
      publish(metricTopic + "value", buf);
      
      DEBUG("Published metric '%s':", METRICS[i].name);
      DEBUG("  Value: %lld", values[i].value);
      DEBUG("  Unit: %d", (int)values[i].unit);
      DEBUG("  Scaler: %d", (int)values[i].scaler);
    }
  }

private:
  MqttConfig config;
  WiFiClient net;
  MQTTClient client;
  bool connected = false;
  String baseTopic;

  void publish(const String &topic, const String &payload)
  {
    publish(topic.c_str(), payload.c_str());
  }
  void publish(String &topic, const char *payload)
  {
    publish(topic.c_str(), payload);
  }
  void publish(const char *topic, const String &payload)
  {
    publish(topic, payload.c_str());
  }
  void publish(const char *topic, const char *payload)
  {
    if (!client.connected())
    {
      connect();
    }
    if (!client.connected())
    {
      // Something failed
      DEBUG("Connection to MQTT broker failed.");
      DEBUG("Unable to publish a message to '%s'.", topic);
      return;
    }
    DEBUG("Publishing message to '%s':", topic);
    DEBUG(payload);
    client.publish(topic, payload);
  }
};

#endif