#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include "config.h"
#include "debug.h"
#include "MQTT.h"
#include <string.h>
#include <sml/sml_file.h>

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

  void publish(Sensor *sensor, sml_file *file)
  {

    for (int i = 0; i < file->messages_len; i++)
    {
        sml_message *message = file->messages[i];
        if (*message->message_body->tag == SML_MESSAGE_GET_LIST_RESPONSE)
        {
            sml_list *entry;
            sml_get_list_response *body;
            body = (sml_get_list_response *)message->message_body->data;
            for (entry = body->val_list; entry != NULL; entry = entry->next)
            {
                if (!entry->value)
                {   // do not crash on null value
                    continue;
                }

                char obisIdentifier[32];
                char buffer[255];

                sprintf(obisIdentifier, "%d-%d:%d.%d.%d",
                        entry->obj_name->str[0], entry->obj_name->str[1],
                        entry->obj_name->str[2], entry->obj_name->str[3],
                        entry->obj_name->str[4]);

                String entryTopic = baseTopic + "sensor/" + (sensor->config->name) + "/obis/" + obisIdentifier + "/";
                
                if (((entry->value->type & SML_TYPE_FIELD) == SML_TYPE_INTEGER) ||
                         ((entry->value->type & SML_TYPE_FIELD) == SML_TYPE_UNSIGNED))
                {
                    double value = sml_value_to_double(entry->value);
                    int scaler = (entry->scaler) ? *entry->scaler : 0;
                    int prec = -scaler;
                    if (prec < 0)
                        prec = 0;
                    value = value * pow(10, scaler);
                    sprintf(buffer, "%.*f", prec, value);
                    publish(entryTopic + "value", buffer);
                }
                else if (!sensor->config->numeric_only) {
                  if (entry->value->type == SML_TYPE_OCTET_STRING)
                  {
                      char *value;
                      sml_value_to_strhex(entry->value, &value, true);
                      publish(entryTopic + "value", value);
                      free(value);
                  }
                  else if (entry->value->type == SML_TYPE_BOOLEAN)
                  {
                      publish(entryTopic + "value",  entry->value->data.boolean ? "true" : "false");
                  }
                }
            }
        }
    }
  }

private:
  MqttConfig config;
  WiFiClient net;
  MQTTClient client = MQTTClient(512);
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
    DEBUG("%s\n", payload);
    client.publish(topic, payload);
  }
};

#endif