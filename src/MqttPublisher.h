#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include "config.h"
#include "debug.h"
#include <Ticker.h>

#include <AsyncMqttClient.h>
#include <string.h>
#include <sml/sml_file.h>

#define MQTT_RECONNECT_DELAY 5
#define MQTT_LWT_TOPIC "LWT"
#define MQTT_LWT_RETAIN true
#define MQTT_LWT_QOS 2
#define MQTT_LWT_PAYLOAD_ONLINE "Online"
#define MQTT_LWT_PAYLOAD_OFFLINE "Offline"

using namespace std;

struct MqttConfig
{
  char server[128] = "mosquitto";
  char port[8] = "1883";
  char username[128] = "";
  char password[128] = "";
  char topic[128] = "iot/smartmeter/";
};

class MqttPublisher
{
public:
  void setup(MqttConfig _config)
  {
    config = _config;
    uint8_t lastCharOfTopic = strlen(config.topic) - 1;
    baseTopic = String(config.topic) + (lastCharOfTopic >= 0 && config.topic[lastCharOfTopic] == '/' ? "" : "/");
    lastWillTopic = String(baseTopic + MQTT_LWT_TOPIC);

    DEBUG(F("MQTT: Setting up..."));
    DEBUG(F("MQTT: Server: %s"),config.server);
    DEBUG(F("MQTT: Port: %d"),atoi(config.port));
    DEBUG(F("MQTT: Username: %s"),config.username);
    DEBUG(F("MQTT: Password: <hidden>"));
    DEBUG(F("MQTT: Topic: %s"), baseTopic.c_str());
    
    client.setServer(const_cast<const char *>(config.server), atoi(config.port));
    if (strlen(config.username) > 0 || strlen(config.password) > 0)
    {
      client.setCredentials(config.username, config.password);
    }
    client.setCleanSession(true);
    client.setWill(lastWillTopic.c_str(), MQTT_LWT_QOS, MQTT_LWT_RETAIN, MQTT_LWT_PAYLOAD_OFFLINE);
    client.setKeepAlive(MQTT_RECONNECT_DELAY * 3);
    this->registerHandlers();
  
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
          { // do not crash on null value
            continue;
          }

          char obisIdentifier[32];
          char buffer[255];

          sprintf(obisIdentifier, "%d-%d:%d.%d.%d/%d",
                  entry->obj_name->str[0], entry->obj_name->str[1],
                  entry->obj_name->str[2], entry->obj_name->str[3],
                  entry->obj_name->str[4], entry->obj_name->str[5]);

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
          else if (!sensor->config->numeric_only)
          {
            if (entry->value->type == SML_TYPE_OCTET_STRING)
            {
              char *value;
              sml_value_to_strhex(entry->value, &value, true);
              publish(entryTopic + "value", value);
              free(value);
            }
            else if (entry->value->type == SML_TYPE_BOOLEAN)
            {
              publish(entryTopic + "value", entry->value->data.boolean ? "true" : "false");
            }
          }
        }
      }
    }
  }

  void connect()
  {
    if (this->connected)
    {
      DEBUG(F("MQTT: Already connected. Aborting connection request."));
      return;
    }
    DEBUG(F("MQTT: Connecting to broker..."));
    client.connect();
  }

  void disconnect()
  {
    if (!this->connected)
    {
      DEBUG(F("MQTT: Not connected. Aborting disconnect request."));
      return;
    }
    DEBUG(F("MQTT: Disconnecting from broker..."));
    client.disconnect();
    this->reconnectTimer.detach();
  }

private:
  bool connected = false;
  MqttConfig config;
  AsyncMqttClient client;
  Ticker reconnectTimer;
  String baseTopic;
  String lastWillTopic;

  void publish(const String &topic, const String &payload, uint8_t qos=0, bool retain=false)
  {
    publish(topic.c_str(), payload.c_str(), qos, retain);
  }
  void publish(String &topic, const char *payload, uint8_t qos=0, bool retain=false)
  {
    publish(topic.c_str(), payload, qos, retain);
  }
  void publish(const char *topic, const String &payload, uint8_t qos=0, bool retain=false)
  {
    publish(topic, payload.c_str(), qos, retain);
  }


  void publish(const char *topic, const char *payload, uint8_t qos=0, bool retain=false)
  {
    if (this->connected)
    {
      DEBUG(F("MQTT: Publishing to %s:"), topic);
      DEBUG(F("%s\n"), payload);
      client.publish(topic, qos, retain, payload, strlen(payload));
    }
  }

  void registerHandlers()
  {

    client.onConnect([this](bool sessionPresent) {
      this->connected = true;
      this->reconnectTimer.detach();
      DEBUG(F("MQTT: Connection established."));
      char message[64];
      snprintf(message, 64, "Hello from %08X, running SMLReader version %s.", ESP.getChipId(), VERSION);
      info(message);
      publish(baseTopic + MQTT_LWT_TOPIC, MQTT_LWT_PAYLOAD_ONLINE, MQTT_LWT_QOS, MQTT_LWT_RETAIN);
    });
    client.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
      this->connected = false;
      DEBUG(F("MQTT: Disconnected. Reason: %d."), reason);
      reconnectTimer.attach(MQTT_RECONNECT_DELAY, [this]() {
        if (WiFi.isConnected()) {
          this->connect();          
        }
      });
    });
  }
};

#endif