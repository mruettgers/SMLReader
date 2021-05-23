#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include "config.h"
#include "debug.h"
#include <Ticker.h>
#include <PangolinMQTT.h>
#include <string.h>
#include <sml/sml_file.h>
#include <Ticker.h>

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

    client.setServer(const_cast<const char *>(config.server), atoi(config.port));
    if (strlen(config.username) > 0 || strlen(config.password) > 0)
    {
      client.setCredentials(config.username, config.password);
    }

    client.setCleanSession(true);
    client.setWill(String(baseTopic + MQTT_LWT_TOPIC).c_str(), MQTT_LWT_QOS, MQTT_LWT_RETAIN, MQTT_LWT_PAYLOAD_OFFLINE);
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
  }

private:
  bool connected = false;
  MqttConfig config;
  WiFiClient net;
  PangolinMQTT client;
  Ticker reconnectTimer;
  String baseTopic;

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
      client.publish(topic, payload, strlen(payload), qos, retain);
    }
  }

  void registerHandlers()
  {
    client.onConnect([this](bool sessionPresent) {
      this->connected = true;
      reconnectTimer.detach();
      DEBUG(F("MQTT client connection established."));
      char message[64];
      snprintf(message, 64, "Hello from %08X, running SMLReader version %s.", ESP.getChipId(), VERSION);
      info(message);
      publish(baseTopic + MQTT_LWT_TOPIC, MQTT_LWT_PAYLOAD_ONLINE, MQTT_LWT_QOS, MQTT_LWT_RETAIN);
    });
    client.onDisconnect([this](int8_t reason) {
      this->connected = false;
      DEBUG("MQTT client disconnected with reason=%d", reason);
      reconnectTimer.attach(MQTT_RECONNECT_DELAY, [this]() {
        this->connect();
      });
    });
    client.onError([this](uint8_t e, uint32_t info) {
      switch (e)
      {
      case TCP_DISCONNECTED:
        // usually because your structure is wrong and you called a function before onMqttConnect
        DEBUG(F("MQTT: NOT CONNECTED info=%d"), info);
        break;
      case MQTT_SERVER_UNAVAILABLE:
        // server has gone away - network problem? server crash?
        DEBUG(F("MQTT: MQTT_SERVER_UNAVAILABLE info=%d"), info);
        break;
      case UNRECOVERABLE_CONNECT_FAIL:
        // there is something wrong with your connection parameters? IP:port incorrect? user credentials typo'd?
        DEBUG(F("MQTT: UNRECOVERABLE_CONNECT_FAIL info=%d"), info);
        break;
      case TLS_BAD_FINGERPRINT:
        DEBUG(F("MQTT: TLS_BAD_FINGERPRINT info=%d"), info);
        break;
      case SUBSCRIBE_FAIL:
        // you tried to subscribe to an invalid topic
        DEBUG(F("MQTT: SUBSCRIBE_FAIL info=%d"), info);
        break;
      case INBOUND_QOS_ACK_FAIL:
        DEBUG(F("MQTT: OUTBOUND_QOS_ACK_FAIL id=%d"), info);
        break;
      case OUTBOUND_QOS_ACK_FAIL:
        DEBUG(F("MQTT: OUTBOUND_QOS_ACK_FAIL id=%d"), info);
        break;
      case INBOUND_PUB_TOO_BIG:
        // someone sent you a p[acket that this MCU does not have enough FLASH to handle
        DEBUG(F("MQTT: INBOUND_PUB_TOO_BIG size=%d Max=%d"), info, client.getMaxPayloadSize());
        break;
      case OUTBOUND_PUB_TOO_BIG:
        // you tried to send a packet that this MCU does not have enough FLASH to handle
        DEBUG(F("MQTT: OUTBOUND_PUB_TOO_BIG size=%d Max=%d"), info, client.getMaxPayloadSize());
        break;
      case BOGUS_PACKET: //  Your server sent a control packet type unknown to MQTT 3.1.1
                         //  99.99% unlikely to ever happen, but this message is better than a crash, non?
        DEBUG(F("MQTT: BOGUS_PACKET info=%02x"), info);
        break;
      case X_INVALID_LENGTH: //  An x function rcvd a msg with an unexpected length: probale data corruption or malicious msg
                             //  99.99% unlikely to ever happen, but this message is better than a crash, non?
        DEBUG(F("MQTT: X_INVALID_LENGTH info=%02x"), info);
        break;
      default:
        DEBUG(F("MQTT: UNKNOWN ERROR: %u extra info %d"), e, info);
        break;
      }
    });
  }
};

#endif